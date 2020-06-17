// Copyright 2020 Dolphin VR Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VirtualBoy/VBPadEmu.h"

#include <array>

#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/Control/Output.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

#include "VirtualBoy/VBPadStatus.h"

static const u16 button_bitmasks[] = {
    VBPAD_BUTTON_A,
    VBPAD_BUTTON_B,
    VBPAD_BUTTON_SELECT,
    VBPAD_BUTTON_START,
};

static const u16 trigger_bitmasks[] = {
    VBPAD_BUTTON_L,
    VBPAD_BUTTON_R,
};

static const u16 left_dpad_bitmasks[] = {VBPAD_LEFT_DPAD_UP, VBPAD_LEFT_DPAD_DOWN,
                                         VBPAD_LEFT_DPAD_LEFT, VBPAD_LEFT_DPAD_RIGHT};
static const u16 right_dpad_bitmasks[] = {VBPAD_RIGHT_DPAD_UP, VBPAD_RIGHT_DPAD_DOWN,
                                          VBPAD_RIGHT_DPAD_LEFT, VBPAD_RIGHT_DPAD_RIGHT};

static const char* const named_buttons[] = {"A", "B", "Select", "Start"};

static const char* const named_triggers[] = {
    // i18n: The left trigger button (labeled L on real controllers)
    _trans("L"),
    // i18n: The right trigger button (labeled R on real controllers)
    _trans("R")};

VBController::VBController(const unsigned int index) : m_index(index)
{
  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  for (const char* named_button : named_buttons)
  {
    const bool is_start = named_button == std::string("Start");
    const ControllerEmu::Translatability translate =
        is_start ? ControllerEmu::Translate : ControllerEmu::DoNotTranslate;
    // i18n: The START/PAUSE button on Virtual Boy controllers
    std::string ui_name = is_start ? _trans("START") : named_button;
    m_buttons->AddInput(translate, named_button, std::move(ui_name));
  }

  // triggers
  groups.emplace_back(m_triggers = new ControllerEmu::Buttons(_trans("Triggers")));
  for (const char* named_trigger : named_triggers)
  {
    m_triggers->AddInput(ControllerEmu::Translate, named_trigger);
  }

  // dpad
  // i18n: The Virtual Boy controller has two DPads, one for each hand, this is the left-hand DPad
  groups.emplace_back(m_left_dpad = new ControllerEmu::Buttons(_trans("Left D-Pad")));
  for (const char* named_direction : named_directions)
  {
    m_left_dpad->AddInput(ControllerEmu::Translate, named_direction);
  }

  // i18n: The Virtual Boy controller has two DPads, one for each hand, this is the left-hand DPad
  groups.emplace_back(m_right_dpad = new ControllerEmu::Buttons(_trans("Right D-Pad")));
  for (const char* named_direction : named_directions)
  {
    m_right_dpad->AddInput(ControllerEmu::Translate, named_direction);
  }
}

std::string VBController::GetName() const
{
  return std::string("VBPad") + char('1' + m_index);
}

ControllerEmu::ControlGroup* VBController::GetGroup(VBPadGroup group)
{
  switch (group)
  {
  case VBPadGroup::Buttons:
    return m_buttons;
  case VBPadGroup::LeftDPad:
    return m_left_dpad;
  case VBPadGroup::RightDPad:
    return m_right_dpad;
  case VBPadGroup::Triggers:
    return m_triggers;
  default:
    return nullptr;
  }
}

VBPadStatus VBController::GetInput() const
{
  const auto lock = GetStateLock();
  VBPadStatus pad = {};

  // buttons
  m_buttons->GetState(&pad.button, button_bitmasks);

  // dpad
  m_left_dpad->GetState(&pad.button, left_dpad_bitmasks);
  m_right_dpad->GetState(&pad.button, right_dpad_bitmasks);

  // triggers
  m_triggers->GetState(&pad.button, trigger_bitmasks);

  return pad;
}

void VBController::LoadDefaults(const ControllerInterface& ciface)
{
  EmulatedController::LoadDefaults(ciface);

  // Buttons
  m_buttons->SetControlExpression(0, "X");  // A
  m_buttons->SetControlExpression(1, "Z");  // B
  m_buttons->SetControlExpression(2, "C");  // Select
#ifdef _WIN32
  m_buttons->SetControlExpression(3, "!LMENU & RETURN");  // Start
#else
  // OS X/Linux
  m_buttons->SetControlExpression(4, "!`Alt_L` & Return");  // Start
#endif

  // C Stick
  m_right_dpad->SetControlExpression(0, "I");  // Up
  m_right_dpad->SetControlExpression(1, "K");  // Down
  m_right_dpad->SetControlExpression(2, "J");  // Left
  m_right_dpad->SetControlExpression(3, "L");  // Right
#ifdef _WIN32
  // D-Pad
  m_left_dpad->SetControlExpression(0, "UP | T");     // Up
  m_left_dpad->SetControlExpression(1, "DOWN | G");   // Down
  m_left_dpad->SetControlExpression(2, "LEFT | F");   // Left
  m_left_dpad->SetControlExpression(3, "RIGHT | H");  // Right
#elif __APPLE__
  // D-Pad
  m_left_dpad->SetControlExpression(0, "Up Arrow | T");     // Up
  m_left_dpad->SetControlExpression(1, "Down Arrow | G");   // Down
  m_left_dpad->SetControlExpression(2, "Left Arrow | F");   // Left
  m_left_dpad->SetControlExpression(3, "Right Arrow | H");  // Right
#else
  // not sure if these are right
  m_left_dpad->SetControlExpression(0, "Up | T");     // Up
  m_left_dpad->SetControlExpression(1, "Down | G");   // Down
  m_left_dpad->SetControlExpression(2, "Left | F");   // Left
  m_left_dpad->SetControlExpression(3, "Right | H");  // Right
#endif

  // Triggers
  m_triggers->SetControlExpression(0, "Q");  // L
  m_triggers->SetControlExpression(1, "W");  // R
}
