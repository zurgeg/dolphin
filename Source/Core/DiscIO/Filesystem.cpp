// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "DiscIO/Filesystem.h"
#include "DiscIO/FileSystemGCWii.h"
#include "DiscIO/FileSystemWiiU.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{

IFileSystem::IFileSystem(const IVolume *_rVolume)
	: m_rVolume(_rVolume)
{}


IFileSystem::~IFileSystem()
{}


IFileSystem* CreateFileSystem(const IVolume* _rVolume)
{
	IFileSystem* pFileSystem = nullptr;
	bool is_wii_u = false;

	if (_rVolume)
	{
		u32 Temp = 0;
		_rVolume->Read(0, 4, (u8*)&Temp, false);
		Temp = Common::swap32(Temp);
		is_wii_u = (Temp == 0x5755502D);
	}

	if (is_wii_u)
		pFileSystem = new CFileSystemWiiU(_rVolume);
	else
		pFileSystem = new CFileSystemGCWii(_rVolume);

	if (!pFileSystem)
		return nullptr;

	if (!pFileSystem->IsValid())
	{
		delete pFileSystem;
		pFileSystem = nullptr;
	}

	return pFileSystem;
}

} // namespace
