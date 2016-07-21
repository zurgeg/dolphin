// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#define OVR_D3D_VERSION 11

#include <windows.h>
#include "VideoCommon/VR.h"
#include "VideoCommon/VR920.h"

#include "d3d11.h"

namespace DX11
{
void VR_ConfigureHMD();
void VR_StartFramebuffer();
void VR_StopFramebuffer();
void VR_RenderToEyebuffer(int eye);
void VR_BeginFrame();
void VR_PresentHMDFrame();
void VR_DrawTimewarpFrame();
}