// Copyright 2020 Dolphin VR Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerInterface/Device.h"

class InputConfig;
enum class VBPadGroup;
struct VBPadStatus;

namespace ControllerEmu
{
class ControlGroup;
}

namespace VBPad
{
void Shutdown();
void Initialize();
void LoadConfig();
bool IsInitialized();

InputConfig* GetConfig();

VBPadStatus GetStatus(int pad_num);
ControllerEmu::ControlGroup* GetGroup(int pad_num, VBPadGroup group);
}  // namespace Pad
