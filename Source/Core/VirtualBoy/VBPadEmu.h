// Copyright 2020 Dolphin VR Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

struct VBPadStatus;

namespace ControllerEmu
{
class Buttons;
class Triggers;
}  // namespace ControllerEmu

enum class VBPadGroup
{
  Buttons,
  LeftDPad,
  RightDPad,
  Triggers
};

class VBController : public ControllerEmu::EmulatedController
{
public:
  explicit VBController(unsigned int index);
  VBPadStatus GetInput() const;

  std::string GetName() const override;

  ControllerEmu::ControlGroup* GetGroup(VBPadGroup group);

  void LoadDefaults(const ControllerInterface& ciface) override;

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::Buttons* m_left_dpad;
  ControllerEmu::Buttons* m_right_dpad;
  ControllerEmu::Buttons* m_triggers;

  const unsigned int m_index;
};
