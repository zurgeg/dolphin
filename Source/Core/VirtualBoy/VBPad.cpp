// Copyright 2020 Dolphin VR Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VirtualBoy/VBPad.h"

#include <cstring>

#include "Common/Common.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"
#include "VirtualBoy/VBPadEmu.h"
#include "VirtualBoy/VBPadStatus.h"

namespace VBPad
{
static InputConfig s_vb_config("VBPad", _trans("VBPad"), "VBPad");
InputConfig* GetConfig()
{
  return &s_vb_config;
}

void Shutdown()
{
  s_vb_config.UnregisterHotplugCallback();

  s_vb_config.ClearControllers();
}

void Initialize()
{
  if (s_vb_config.ControllersNeedToBeCreated())
  {
    s_vb_config.CreateController<VBController>(0);
  }

  s_vb_config.RegisterHotplugCallback();

  // Load the saved controller config
  s_vb_config.LoadConfig(true);
}

void LoadConfig()
{
  s_vb_config.LoadConfig(true);
}

bool IsInitialized()
{
  return !s_vb_config.ControllersNeedToBeCreated();
}

VBPadStatus GetStatus(int pad_num)
{
  return static_cast<VBController*>(s_vb_config.GetController(pad_num))->GetInput();
}

ControllerEmu::ControlGroup* GetGroup(int pad_num, VBPadGroup group)
{
  return static_cast<VBController*>(s_vb_config.GetController(pad_num))->GetGroup(group);
}

}  // namespace VBPad
