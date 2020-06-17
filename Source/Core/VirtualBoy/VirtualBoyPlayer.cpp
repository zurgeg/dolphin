// Copyright 2020 Dolphin VR Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VirtualBoy/VirtualBoyPlayer.h"

#include <algorithm>
#include <imgui.h>
#include <mutex>

#include <fmt/format.h>

#include <mednafen/mednafen-types.h>
#include <mednafen/vb/vb.h>
#include <mednafen/vb/vip.h>
#include <mednafen/vrvb.h>

#include "AudioCommon/AudioCommon.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/Host.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/RenderBase.h"
#include "VirtualBoy/VBPad.h"
#include "VirtualBoy/VBPadStatus.h"

struct TVBWindow
{
  u16 bgmap_base : 4;
  u16 padding : 2;
  u16 end : 1;
  u16 ovr : 1;
  u16 scy : 2;
  u16 scx : 2;
  u16 bgm : 2;
  u16 ron : 1;
  u16 lon : 1;
  s16 gx : 11;
  s16 pad_gx : 5;
  s16 gp : 9;
  s16 pad_gp : 7;
  s16 gy : 11;
  s16 pad_gy : 5;
  s16 mx;
  s16 mp : 9;
  s16 pad_mp : 7;
  s16 my;
  s16 width : 11;
  s16 pad_width : 5;
  u16 height : 10;
  u16 pad_height : 6;
  u16 padding2 : 4;
  u16 param_base : 12;
  u16 overplane_chr : 11;
  u16 overplane_pad : 3;
  u16 overplane_pal : 2;
  u16 forbidden[5];
};
static_assert(sizeof(TVBWindow) == 16 * sizeof(u16), "Wrong size for TVBWindow");

static constexpr int INVALID_TILE = 2048;

struct TVBBackgroundMap
{
  u8 bg_base;
  u8 bgm;
  u8 xmaps, ymaps;
  int minx, miny, maxx, maxy;
  // list of windows that it's used in?

  // tile data
  std::vector<u16> tiles;
  void SetTile(int x, int y, u16 tile);
  u16 GetTile(int x, int y);
  void Clear();
  void Update();
};

VirtualBoyPlayer::VirtualBoyPlayer()
{
}

VirtualBoyPlayer::~VirtualBoyPlayer()
{
}

bool VirtualBoyPlayer::Open(const std::string& filename)
{
  Close();

  bool result = File::ReadFileToString(filename, m_rom);
  if (result)
  {
    m_filename = filename;
    VRVB::video_cb = VirtualBoyVideoCallback;
    VRVB::audio_cb = VirtualBoyAudioCallback;
    VRVB::LoadRom((const u8*)m_rom.c_str(), m_rom.size());
    LoadCartridgeRam();
    VRVB::Init();
    memset(g_bg_tiles_modified, false, sizeof(g_bg_tiles_modified));
    if (m_FileLoadedCb)
      m_FileLoadedCb();
  }

  return result;
}

void VirtualBoyPlayer::Close()
{
  if (m_rom.empty())
    return;
  SaveCartridgeRam();
  VRVB::g_last_save_time = 0;
  m_filename = "";
  m_rom = "";
}

bool VirtualBoyPlayer::IsPlaying() const
{
  return (!m_rom.empty()) && Core::IsRunning();
}

class VirtualBoyPlayer::CPUCore final : public CPUCoreBase
{
public:
  explicit CPUCore(VirtualBoyPlayer* parent) : m_parent(parent) {}
  CPUCore(const CPUCore&) = delete;
  ~CPUCore() {}
  CPUCore& operator=(const CPUCore&) = delete;

  void Init() override {}

  void Shutdown() override {}
  void ClearCache() override
  {
    // Nothing to clear.
  }

  void SingleStep() override
  {
    // NOTE: AdvanceFrame() will get stuck forever in Dual Core because the FIFO
    //   is disabled by CPU::EnableStepping(true) so the frame never gets displayed.
    PanicAlertT("Cannot SingleStep the Virtual Boy. Use Frame Advance instead.");
  }

  const char* GetName() const override { return "VirtualBoyPlayer"; }
  void Run() override
  {
    while (CPU::GetState() == CPU::State::Running)
    {
      switch (m_parent->AdvanceFrame())
      {
      case CPU::State::PowerDown:
        CPU::Break();
        Host_Message(HostMessageID::WMUserStop);
        break;

      case CPU::State::Stepping:
        CPU::Break();
        Host_UpdateMainFrame();
        break;

      case CPU::State::Running:
        break;
      }
    }
  }

private:
  VirtualBoyPlayer* m_parent;
};

CPU::State VirtualBoyPlayer::AdvanceFrame()
{
  if (m_reset)
  {
    m_reset = false;
    VRVB::Reset();
  }
  // frame timing
  u64 current_time = Common::Timer::GetTimeUs();
  if (m_old_time == 0 || m_old_time > current_time)
    m_old_time = current_time;
  m_time_since_frame += current_time - m_old_time;
  u64 frame_length = 1000000 / 50;
  float speed_multiplier = SConfig::GetInstance().m_EmulationSpeed;
  if (speed_multiplier == 0)
    frame_length = 0;
  else
    frame_length = frame_length / speed_multiplier;
  if (m_time_since_frame < frame_length)
  {
    MicroSleep(frame_length - m_time_since_frame - 10);
  }
  m_old_time = current_time;
  current_time = Common::Timer::GetTimeUs();
  m_time_since_frame += current_time - m_old_time;
  m_old_time = current_time;
  m_time_since_frame -= frame_length;

  g_controller_interface.UpdateInput();
  VBPadStatus pad_status = VBPad::GetStatus(0);

  VRVB::input_buf[0] = 0;
  VRVB::input_buf[0] = pad_status.button;
#if 0
  // For now, get inputs from the GameCube controller's mapping
  if (pad_status.button & PAD_BUTTON_A)
    VRVB::input_buf[0] |= 1 << 0;
  if (pad_status.button & PAD_BUTTON_B)
    VRVB::input_buf[0] |= 1 << 1;
  if (pad_status.button & PAD_TRIGGER_R)
    VRVB::input_buf[0] |= 1 << 2;
  if (pad_status.button & PAD_TRIGGER_L)
    VRVB::input_buf[0] |= 1 << 3;
  if (pad_status.button & (PAD_BUTTON_X | PAD_BUTTON_START))
    VRVB::input_buf[0] |= 1 << 10;
  if (pad_status.button & PAD_BUTTON_Y)
    VRVB::input_buf[0] |= 1 << 11;

  if (pad_status.substickX < 64)
    VRVB::input_buf[0] |= 1 << 12;
  if (pad_status.substickX > 196)
    VRVB::input_buf[0] |= 1 << 5;
  if (pad_status.substickY > 196)
    VRVB::input_buf[0] |= 1 << 4;
  if (pad_status.substickY < 64)
    VRVB::input_buf[0] |= 1 << 13;

  if (pad_status.stickX < 64)
    VRVB::input_buf[0] |= 1 << 7;
  if (pad_status.stickX > 196)
    VRVB::input_buf[0] |= 1 << 6;
  if (pad_status.stickY > 196)
    VRVB::input_buf[0] |= 1 << 9;
  if (pad_status.stickY < 64)
    VRVB::input_buf[0] |= 1 << 8;
  if (pad_status.button & PAD_BUTTON_RIGHT)
    VRVB::input_buf[0] |= 1 << 6;
  if (pad_status.button & PAD_BUTTON_LEFT)
    VRVB::input_buf[0] |= 1 << 7;
  if (pad_status.button & PAD_BUTTON_DOWN)
    VRVB::input_buf[0] |= 1 << 8;
  if (pad_status.button & PAD_BUTTON_UP)
    VRVB::input_buf[0] |= 1 << 9;
#endif

  VRVB::Run();

  // Save every five minutes
  if (VRVB::g_cartridge_ram_modified &&
      Common::Timer::GetTimeMs() - VRVB::g_last_save_time > 5 * 60 * 1000)
  {
    SaveCartridgeRam();
  }

  return CPU::State::Running;
}

std::unique_ptr<CPUCoreBase> VirtualBoyPlayer::GetCPUCore()
{
  return std::make_unique<CPUCore>(this);
}

void VirtualBoyPlayer::SetFileLoadedCallback(CallbackFunc callback)
{
  m_FileLoadedCb = std::move(callback);

  // Trigger the callback immediatly if the file is already loaded.
  if (!m_rom.empty())
  {
    m_FileLoadedCb();
  }
}

VirtualBoyPlayer& VirtualBoyPlayer::GetInstance()
{
  static VirtualBoyPlayer instance;
  return instance;
}

void VirtualBoyPlayer::LoadState(const std::string& filename)
{
  std::string state_contents;
  if (File::ReadFileToString(filename, state_contents))
  {
    VRVB::retro_unserialize(state_contents.data(), state_contents.length());
    Core::DisplayMessage(fmt::format("Loaded state from {}", filename), 2000);
  }
  else
  {
    Core::DisplayMessage("State not found", 2000);
  }
}

void VirtualBoyPlayer::LoadStateFromBuffer(std::vector<u8>& buffer)
{
  VRVB::retro_unserialize(buffer.data(), buffer.size());
}

void VirtualBoyPlayer::SaveState(const std::string& filename)
{
  std::string state_contents;
  // get the size of the savestate
  size_t size = VRVB::retro_serialize_size();
  if (size > 0)
  {
    Core::DisplayMessage("Saving State...", 1000);
    state_contents.resize(size);
    VRVB::retro_serialize((void*)state_contents.c_str(), size);
    File::WriteStringToFile(filename, state_contents);
    Core::DisplayMessage(fmt::format("Saved State to {}", filename), 2000);
  }
}

void VirtualBoyPlayer::SaveStateToBuffer(std::vector<u8>& buffer)
{
  // get the size of the savestate
  size_t size = VRVB::retro_serialize_size();
  if (size > 0)
  {
    buffer.resize(size);
    VRVB::retro_serialize((void*)buffer.data(), size);
  }
}

void VirtualBoyPlayer::SaveCartridgeRam()
{
  std::string filename = fmt::format("{}{}.srm", File::GetUserPath(D_CARTSAVES_IDX),
                                     SConfig::GetInstance().GetGameID());
  std::string sram;
  sram.resize(VRVB::save_ram_size());
  memcpy(sram.data(), VRVB::save_ram(), VRVB::save_ram_size());

  if (File::WriteStringToFile(filename, sram))
  {
    VRVB::g_cartridge_ram_modified = false;
    VRVB::g_last_save_time = Common::Timer::GetTimeMs();
    INFO_LOG(VB, "finished loading ram");
  }
}

void VirtualBoyPlayer::LoadCartridgeRam()
{
  std::string filename = fmt::format("{}{}.srm", File::GetUserPath(D_CARTSAVES_IDX),
                                     SConfig::GetInstance().GetGameID());
  std::string sram;
  if (File::ReadFileToString(filename, sram))
  {
    if (sram.length() != (int)VRVB::save_ram_size())
    {
      ERROR_LOG(VB, "ERROR loaded ram size is wrong");
    }
    else
    {
      memcpy(VRVB::save_ram(), sram.data(), VRVB::save_ram_size());
      INFO_LOG(VB, "finished loading ram");
    }
  }
}

void VirtualBoyPlayer::MicroSleep(s64 microseconds)
{
  if (microseconds < 0)
    return;

  static double ticks_per_microsecond = 0;
  static bool initialized = false;
  if (!initialized)
  {
    LARGE_INTEGER ticks_per_second;
    if (QueryPerformanceFrequency(&ticks_per_second))
      ticks_per_microsecond = (double)ticks_per_second.QuadPart / 1000000.0;

    initialized = true;
  }
  int sleep_milliseconds = int((microseconds / 1000) - 3);
  u64 wait_ticks = microseconds * ticks_per_microsecond;
  LARGE_INTEGER start_ticks;
  if (QueryPerformanceCounter(&start_ticks))
  {
    u64 stop_ticks = start_ticks.QuadPart + wait_ticks;
    if (sleep_milliseconds > 0)
      Common::SleepCurrentThread(sleep_milliseconds);
    while (true)
    {
      LARGE_INTEGER current_ticks;
      if (!QueryPerformanceCounter(&current_ticks) || current_ticks.QuadPart >= stop_ticks)
        break;
    }
    return;
  }
  Common::SleepCurrentThread(microseconds / 1000);
}

void VirtualBoyPlayer::LayerImGui()
{
  if (!IsPlaying())
    return;
  constexpr float DEFAULT_WINDOW_WIDTH = 220.0f;
  constexpr float DEFAULT_WINDOW_HEIGHT = 450.0f;
  auto lock = g_renderer->GetImGuiLock();
  const float scale = ImGui::GetIO().DisplayFramebufferScale.x;

  ImGui::SetNextWindowPos(ImVec2(10.0f * scale, 10.0f * scale), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(DEFAULT_WINDOW_WIDTH * scale, DEFAULT_WINDOW_HEIGHT * scale),
      ImGui::GetIO().DisplaySize);

  if (!ImGui::Begin("Virtual Boy", nullptr, ImGuiWindowFlags_None))
  {
    ImGui::End();
    return;
  }

  if (!DisplayActive)
  {
    ImGui::Text("Off");
  }
  else if ((XPCTRL & 2) == 0)
  {
    ImGui::Text("Manual pixel rendering");
  }
  else
  {
    ImGui::Text("Background Colour: %x", BKCOL);
    bool skip_next = false;
    for (int world = 31; world >= 0; world--)
    {
      if (skip_next)
      {
        skip_next = false;
        continue;
      }
      TVBWindow* win = (TVBWindow*)&DRAM[0x1D800 / 2] + world;
      if (win->end)
        break;
      if (!win->lon && !win->ron)
      {
        ImGui::Checkbox(fmt::format("{}: off", world).c_str(), &g_debug_show_world[world]);
        continue;
      }
      uint32 xmaps = 1 << win->scx;
      uint32 ymaps = 1 << win->scy;
      uint32 param_base = win->param_base & 0xFFF0;
      const char* modes[4] = {"", "Wavy", "Matrix", "Sprites"};
      std::string s;
      // if this is the right eye and the next (going backwards) is the left eye
      TVBWindow* win_left = win;
      TVBWindow* win_right = win;
      if (world > 0 && !win[-1].end && win[0].bgm == win[-1].bgm && win[0].gy == win[-1].gy)
      {
        if (!win[0].lon && win[-1].lon && !win[-1].ron)
          win_left = win - 1;
        else if (!win[0].ron && win[-1].ron && !win[-1].lon)
          win_right = win - 1;
      }
      if (win_left != win_right)
      {
        skip_next = true;
        s = "LR ";
      }
      else if (!win->ron)
      {
        s = "L ";
      }
      else if (!win->lon)
      {
        s = "R ";
      }
      s16 mz = (win_right->mx + win_right->mp) - (win_left->mx - win_left->mp);
      s16 gz = (win_right->gx + win_right->gp) - (win_left->gx - win_left->gp);
      s16 z = mz - gz;

      s += fmt::format("{}: {}", world, modes[win->bgm]);
      if (win->bgm != 3)
      {
        s += "BG(";
        for (int y = 0; y < ymaps; y++)
        {
          if (y > 0)
            s += "; ";
          for (int x = 0; x < xmaps; x++)
          {
            if (x > 0)
              s += ", ";
            s += fmt::format("{}", win->bgmap_base + y * xmaps + x);
          }
        }
        s += ")";
        if (xmaps != 1 || ymaps != 1)
          s += fmt::format("={}x{}", xmaps * 64 * 8, ymaps * 64 * 8);
        if (win->ovr)
          s += fmt::format(" o={},{}", (int)win->overplane_chr, (int)win->overplane_pal);
        s += fmt::format(" z={}", z);  // total pixels of separation, ie. double the parallax value
        if (win->width != 384 - 1 && win->height != 224 - 1)
        {
          s += fmt::format(" ({}, {})-({}, {})", (int)win->gx, (int)win->gy,
                           (int)win->gx + win->width, (int)win->gy + win->height);
        }
        else if (win->height != 224 - 1)
        {
          s += fmt::format(" (y {}-{})", (int)win->gy, (int)win->gy + win->height);
        }
        else if (win->width != 384 - 1)
        {
          s += fmt::format(" (x {}-{})", (int)win->gx, (int)win->gx + win->width);
        }
        else if (win->gx || win->gy)
        {
          s += fmt::format(" ({}, {})", (int)win->gx, (int)win->gy);
        }
        if (win->mx != win->gx || win->my != win->gy)
          s += fmt::format(" scroll({},{})", win->mx - win->gx, win->my - win->gy);
      }
      ImGui::Checkbox(s.c_str(), &g_debug_show_world[world]);
      if (skip_next)
        g_debug_show_world[world - 1] = g_debug_show_world[world];
    }

#if 0
      const uint16* world_ptr = &DRAM[(0x1D800 + world * 0x20) >> 1];
      bool end = world_ptr[0] & 0x40;
      if (end)
        break;

      uint32 bgmap_base = world_ptr[0] & 0xF;
      bool over = world_ptr[0] & 0x80;
      uint32 scy = (world_ptr[0] >> 8) & 3;
      uint32 scx = (world_ptr[0] >> 10) & 3;
      uint32 xmaps = 1 << scx;
      uint32 ymaps = 1 << scy;
      uint32 bgm = (world_ptr[0] >> 12) & 3;
      bool lron[2] = {(bool)(world_ptr[0] & 0x8000), (bool)(world_ptr[0] & 0x4000)};

      s16 gx = sign_11_to_s16(world_ptr[1]);
      s16 gp = sign_9_to_s16(world_ptr[2]);
      s16 gy = sign_11_to_s16(world_ptr[3]);
      s16 mx = world_ptr[4];
      s16 mp = sign_9_to_s16(world_ptr[5]);
      s16 my = world_ptr[6];
      s16 window_width = sign_11_to_s16(world_ptr[7]) + 1;
      s16 window_height = (world_ptr[8] & 0x3FF) + 1;
      uint32 param_base = (world_ptr[9] & 0xFFF0);
      uint16 overplane_char = world_ptr[10];
      const char* modes[4] = {"", "Wavy", "Matrix", "Sprites"};

      if (lron[0] || lron[1])
      {
        std::string s;
        if (!lron[1])
          s = "L ";
        else if (!lron[0])
          s = "R ";
        s += fmt::format("{}: {}", world, modes[bgm]);
        if (bgm != 3)
        {
          s += "BG(";
          for (int y=0; y<ymaps;y++)
          {
            if (y > 0)
              s += "; ";
            for (int x=0; x<xmaps;x++)
            {
              if (x > 0)
                s += ", ";
              s += fmt::format("{}", bgmap_base + y * xmaps + x);
            }
          }
          s += fmt::format(")={}x{}", xmaps*64*8, ymaps*64*8);
          if (over)
            s += fmt::format(" o={}", overplane_char);
          if (mp)
            s += fmt::format(" z={} ({},{})", mp - gp, mp, -gp);
          else
            s += fmt::format(" z={}", -gp);
          if (window_width !=384 || window_height != 224)
          {
            s += fmt::format(" ({}, {})-({}, {})", gx, gy, gx+window_width-1, gy+window_height-1);
          }
          else if (gx || gy)
          {
            s += fmt::format(" ({}, {})", gx, gy);
          }
          if (mx!=gx || my!=gy)
            s += fmt::format(" scroll({},{})", mx-gx, my-gy);
        }
        ImGui::Checkbox(s.c_str(), &g_debug_show_world[world]);
      }
    }
#endif
  }
  ImGui::End();
  memset(g_bg_tiles_modified, false, sizeof(g_bg_tiles_modified));
}

void VirtualBoyPlayer::ProcessModifiedBG()
{
  static int frame = 0;
  int first_x = 64;
  int last_x = -1;
  for (int bg = 0; bg < 14; bg++)
  {
    u32 offset = 0x2000 * bg;
    u16* bg_start = &DRAM[offset / 2];
    bool* modified = &g_bg_tiles_modified[offset / 2];
    u8 column[64]{};
    bool cleared_entire_bg = true;
    int count = 0;
    for (int x = 0; x < 64; x++)
    {
      for (int y = 0; y < 64; y++)
      {
        if (modified[y * 64 + x])
        {
          column[x]++;
        }
      }
      if (column[x])
      {
        count++;
        if (x < first_x)
          first_x = x;
        if (x > last_x)
          last_x = x;
      }
      else
      {
        cleared_entire_bg = false;
      }
    }
    // if we updated the entire 64
    if (cleared_entire_bg)
    {
      //   mark everything as garbage until it has been seen
    }
    else if (count > 48)
    {
      // if we updated more than we can see, but not the entire 64
      //   mark everything updated as good
    }

    // if we updated a column or two
    //   mark everything updated as good
  }
  frame++;
}

void VirtualBoyPlayer::BGImGui()
{
  if (!IsPlaying())
    return;
  constexpr float DEFAULT_WINDOW_WIDTH = 100.0f;
  constexpr float DEFAULT_WINDOW_HEIGHT = 800.0f;
  auto lock = g_renderer->GetImGuiLock();
  const float scale = ImGui::GetIO().DisplayFramebufferScale.x;

  ImGui::SetNextWindowPos(ImVec2(10.0f * scale, 10.0f * scale), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(DEFAULT_WINDOW_WIDTH * scale, DEFAULT_WINDOW_HEIGHT * scale),
      ImGui::GetIO().DisplaySize);

  if (!ImGui::Begin("VB Background", nullptr, ImGuiWindowFlags_None))
  {
    ImGui::End();
    return;
  }

  static int bg = 0;
  ImGui::InputInt("BG ", &bg);
  if (bg < 0)
    bg = 0;
  if (bg > 13)
    bg = 13;
  u32 offset = 0x2000 * bg;
  u16* bg_start = &DRAM[offset / 2];
  bool* modified_start = &g_bg_tiles_modified[offset / 2];

  ImGui::Columns(64);
  for (int col = 0; col < 64; ++col)
  {
    for (int row = 0; row < 64; ++row)
    {
      u32 index = row * 64 + col;
      u16 CHR = bg_start[index] & 0x7FF;
      int pal = bg_start[index] >> 14;
      bool modified = modified_start[index];
      float brightness = 0;
      float opacity = 0;
      for (int y = 0; y < 8; y++)
      {
        u16 pixels = CHR_RAM[CHR * 8 + y];
        while (pixels > 0)
        {
          int colour = pixels & 3;
          if (colour != 0)
          {
            opacity += 1;
            colour = GPLT_Cache[pal][colour];
            brightness += colour;
          }
          pixels = pixels >> 2;
        }
      }
      brightness = brightness / (8 * 8 * 3);
      opacity = opacity / 8 * 8;
      if (modified)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), fmt::format("{:03x}", CHR).c_str());
      else
        ImGui::TextColored(ImVec4(brightness, 0.3f, 0.3f, 0.3f + opacity * 0.7f),
                           fmt::format("{:03x}", CHR).c_str());
    }
    ImGui::NextColumn();
  }
  ImGui::Columns(1);
  ImGui::End();
}

void __cdecl VirtualBoyAudioCallback(int16_t* SoundBuf, int32_t SoundBufSize)
{
  for (int i = 0; i < SoundBufSize * 2; i++)
    SoundBuf[i] = Common::swap16(SoundBuf[i]);
  g_sound_stream->GetMixer()->PushOtherSamples(SoundBuf, SoundBufSize * 2, 48000);
}

void __cdecl VirtualBoyVideoCallback(const void* data, unsigned width, unsigned height)
{
  if (VirtualBoyPlayer::GetInstance().m_show_layer_window)
    VirtualBoyPlayer::GetInstance().LayerImGui();
  if (VirtualBoyPlayer::GetInstance().m_show_bg_window)
    VirtualBoyPlayer::GetInstance().BGImGui();
  g_renderer->SwapPixelBuffer((const u8*)data, width, height,
                              ((const u8*)data) + 4 * width * (height + 12));
}
