// Copyright 2020 Dolphin VR Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

enum VBPadButton
{
  VBPAD_BUTTON_A = 1 << 0,
  VBPAD_BUTTON_B = 1 << 1,

  VBPAD_BUTTON_R = 1 << 2,
  VBPAD_BUTTON_L = 1 << 3,

  VBPAD_RIGHT_DPAD_UP = 1 << 4,
  VBPAD_RIGHT_DPAD_RIGHT = 1 << 5,

  VBPAD_LEFT_DPAD_RIGHT = 1 << 6,
  VBPAD_LEFT_DPAD_LEFT = 1 << 7,
  VBPAD_LEFT_DPAD_DOWN = 1 << 8,
  VBPAD_LEFT_DPAD_UP = 1 << 9,

  VBPAD_BUTTON_START = 1 << 10,
  VBPAD_BUTTON_SELECT = 1 << 11,

  VBPAD_RIGHT_DPAD_LEFT = 1 << 12,
  VBPAD_RIGHT_DPAD_DOWN = 1 << 13,
};

struct VBPadStatus
{
  u16 button;       // Or-ed PAD_BUTTON_* and PAD_TRIGGER_* bits
};
