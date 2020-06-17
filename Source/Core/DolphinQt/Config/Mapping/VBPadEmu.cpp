// Copyright 2020 Dolphin VR Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/VBPadEmu.h"

#include <QGridLayout>
#include <QGroupBox>

#include "VirtualBoy/VBPad.h"
#include "VirtualBoy/VBPadEmu.h"

#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/InputConfig.h"

VBPadEmu::VBPadEmu(MappingWindow* window) : MappingWidget(window)
{
  CreateMainLayout();
}

void VBPadEmu::CreateMainLayout()
{
  auto* layout = new QGridLayout;

  layout->addWidget(CreateGroupBox(tr("Buttons"), VBPad::GetGroup(GetPort(), VBPadGroup::Buttons)), 0,
                    0);
  layout->addWidget(CreateGroupBox(tr("Left D-Pad"), VBPad::GetGroup(GetPort(), VBPadGroup::LeftDPad)), 0, 1, -1,
                    1);
  layout->addWidget(
      CreateGroupBox(tr("Right D-Pad"), VBPad::GetGroup(GetPort(), VBPadGroup::RightDPad)), 0, 2, -1,
      1);
  layout->addWidget(
      CreateGroupBox(tr("Triggers"), VBPad::GetGroup(GetPort(), VBPadGroup::Triggers)), 0,
                    4);

  setLayout(layout);
}

void VBPadEmu::LoadSettings()
{
  VBPad::LoadConfig();
}

void VBPadEmu::SaveSettings()
{
  VBPad::GetConfig()->SaveConfig();
}

InputConfig* VBPadEmu::GetConfig()
{
  return VBPad::GetConfig();
}
