// Copyright 2020 Dolphin VR Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/CPUCoreBase.h"

namespace CPU
{
enum class State;
}

class VirtualBoyPlayer
{
public:
  using CallbackFunc = std::function<void()>;

  ~VirtualBoyPlayer();

  bool Open(const std::string& filename);
  void Close();

  // Returns a CPUCoreBase instance that can be injected into PowerPC as a
  // pseudo-CPU. The instance is only valid while the VirtualBoyPlayer is Open().
  // Returns nullptr if the VirtualBoyPlayer is not initialized correctly.
  // Play/Pause/Stop of the VirtualBoy can be controlled normally via the
  // PowerPC state.
  std::unique_ptr<CPUCoreBase> GetCPUCore();

  bool IsPlaying() const;

  void SetFileLoadedCallback(CallbackFunc callback);
  void SetFrameWrittenCallback(CallbackFunc callback) {}
  static VirtualBoyPlayer& GetInstance();
  void LoadState(const std::string& filename);
  void LoadStateFromBuffer(std::vector<u8>& buffer);
  void SaveState(const std::string& filename);
  void SaveStateToBuffer(std::vector<u8>& buffer);
  void SaveCartridgeRam();
  void LoadCartridgeRam();
  void Reset() { m_reset = true; }
  void MicroSleep(s64 microseconds);

  // Debugging
  void LayerImGui();
  void ProcessModifiedBG();
  void BGImGui();

  bool m_show_layer_window = false;
  bool m_show_bg_window = false;

private:
  class CPUCore;

  VirtualBoyPlayer();
  CPU::State AdvanceFrame();

  CallbackFunc m_FileLoadedCb = nullptr;
  CallbackFunc m_FrameWrittenCb = nullptr;
  std::string m_rom;
  std::string m_filename;
  bool m_reset = false;
  u64 m_old_time = 0, m_time_since_frame = (u64)1000000 / 50;
};

void __cdecl VirtualBoyAudioCallback(int16_t* SoundBuf, int32_t SoundBufSize);
void __cdecl VirtualBoyVideoCallback(const void* data, unsigned width, unsigned height);
