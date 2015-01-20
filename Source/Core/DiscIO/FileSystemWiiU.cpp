// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "DiscIO/Filesystem.h"
#include "DiscIO/FileSystemWiiU.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
CFileSystemWiiU::CFileSystemWiiU(const IVolume *_rVolume)
	: IFileSystem(_rVolume)
	, m_Initialized(false)
	, m_Valid(false)
{
	m_Valid = DetectFileSystem();
}

CFileSystemWiiU::~CFileSystemWiiU()
{
	m_FileInfoVector.clear();
}

u64 CFileSystemWiiU::GetFileSize(const std::string& _rFullPath)
{
	if (!m_Initialized)
		InitFileSystem();

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);

	if (pFileInfo != nullptr && !pFileInfo->IsDirectory())
		return pFileInfo->m_FileSize;

	return 0;
}

const std::string CFileSystemWiiU::GetFileName(u64 _Address)
{
	if (!m_Initialized)
		InitFileSystem();

	for (auto& fileInfo : m_FileInfoVector)
	{
		if ((fileInfo.m_Offset <= _Address) &&
		    ((fileInfo.m_Offset + fileInfo.m_FileSize) > _Address))
		{
			return fileInfo.m_FullPath;
		}
	}

	return "";
}

u64 CFileSystemWiiU::ReadFile(const std::string& _rFullPath, u8* _pBuffer, size_t _MaxBufferSize)
{
	if (!m_Initialized)
		InitFileSystem();

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);
	if (pFileInfo == nullptr)
		return 0;

	if (pFileInfo->m_FileSize > _MaxBufferSize)
		return 0;

	DEBUG_LOG(DISCIO, "Filename: %s. Offset: %" PRIx64 ". Size: %" PRIx64, _rFullPath.c_str(),
		pFileInfo->m_Offset, pFileInfo->m_FileSize);

	m_rVolume->Read(pFileInfo->m_Offset, pFileInfo->m_FileSize, _pBuffer, true);
	return pFileInfo->m_FileSize;
}

bool CFileSystemWiiU::ExportFile(const std::string& _rFullPath, const std::string& _rExportFilename)
{
	if (!m_Initialized)
		InitFileSystem();

	const SFileInfo* pFileInfo = FindFileInfo(_rFullPath);

	if (!pFileInfo)
		return false;

	u64 remainingSize = pFileInfo->m_FileSize;
	u64 fileOffset = pFileInfo->m_Offset;

	File::IOFile f(_rExportFilename, "wb");
	if (!f)
		return false;

	bool result = true;

	while (remainingSize)
	{
		// Limit read size to 128 MB
		size_t readSize = std::min<size_t>(remainingSize, (u64)0x08000000);

		std::vector<u8> buffer(readSize);

		result = m_rVolume->Read(fileOffset, readSize, &buffer[0], true);

		if (!result)
			break;

		f.WriteBytes(&buffer[0], readSize);

		remainingSize -= readSize;
		fileOffset += readSize;
	}

	return result;
}

bool CFileSystemWiiU::ExportApploader(const std::string& _rExportFolder) const
{
	return false;
}

u32 CFileSystemWiiU::GetBootDOLSize() const
{
	return 0;
}

bool CFileSystemWiiU::GetBootDOL(u8* &buffer, u32 DolSize) const
{
	return false;
}

bool CFileSystemWiiU::ExportDOL(const std::string& _rExportFolder) const
{
	return false;
}

u32 CFileSystemWiiU::Read32(u64 _Offset) const
{
	u32 Temp = 0;
	m_rVolume->Read(_Offset, 4, (u8*)&Temp, true);
	return Common::swap32(Temp);
}

// There seems to be a bug here where it crosses a cluster boundary.
std::string CFileSystemWiiU::GetStringFromOffset(u64 _Offset) const
{
	std::string data;
	data.resize(255);
	m_rVolume->Read(_Offset, data.size(), (u8*)&data[0], true);
	data.erase(std::find(data.begin(), data.end(), 0x00), data.end());

	// TODO: Should we really always use SHIFT-JIS?
	// It makes some filenames in Pikmin (NTSC-U) sane, but is it correct?
	return SHIFTJISToUTF8(data);
}

size_t CFileSystemWiiU::GetFileList(std::vector<const SFileInfo *> &_rFilenames)
{
	if (!m_Initialized)
		InitFileSystem();

	if (_rFilenames.size())
		PanicAlert("CFileSystemWiiU::GetFileList : input list has contents?");
	_rFilenames.clear();
	_rFilenames.reserve(m_FileInfoVector.size());

	for (auto& fileInfo : m_FileInfoVector)
		_rFilenames.push_back(&fileInfo);
	return m_FileInfoVector.size();
}

const SFileInfo* CFileSystemWiiU::FindFileInfo(const std::string& _rFullPath)
{
	if (!m_Initialized)
		InitFileSystem();

	for (auto& fileInfo : m_FileInfoVector)
	{
		if (!strcasecmp(fileInfo.m_FullPath.c_str(), _rFullPath.c_str()))
			return &fileInfo;
	}

	return nullptr;
}

bool CFileSystemWiiU::DetectFileSystem()
{
	return true;
}

void CFileSystemWiiU::InitFileSystem()
{
	typedef struct 
	{
		u32 StartingCluster, ClustersBetweenRepeats, Unknown;
	} THeader;

	m_Initialized = true;

	// magic "FST"
	if (Read32(0x8000) != 0x46535400)
		return;
	// read the whole FST
	u32 HeaderSize = Read32(0x8004);
	u32 HeaderCount = Read32(0x8008);
	u64 FSTOffset = (u64)0x8000 + 0x20 + HeaderSize * HeaderCount;

	// read all fileinfos
	SFileInfo Root;
	Root.m_NameOffset = Read32(FSTOffset + 0x0);
	Root.m_Offset     = (u64)Read32(FSTOffset + 0x4);
	Root.m_FileSize   = Read32(FSTOffset + 0x8);
	Root.m_Unknown    = Read32(FSTOffset + 0xC);

	// check for valid FST
	if (Root.IsDirectory())
	{
		if (m_FileInfoVector.size())
			PanicAlert("Wtf?");

		// read all headers
		THeader *header = new THeader[HeaderCount];
		for (u32 i = 0; i < HeaderCount; ++i)
		{
			header[i].StartingCluster = Read32(0x8000 + 0x20 + i*HeaderSize + 0x0);
			header[i].ClustersBetweenRepeats = Read32(0x8000 + 0x20 + i*HeaderSize + 0x4);
			header[i].Unknown = Read32(0x8000 + 0x20 + i*HeaderSize + 0x14);
		}

		m_FileInfoVector.reserve((unsigned int)Root.m_FileSize);
		for (u32 i = 0; i < Root.m_FileSize; ++i)
		{
			SFileInfo sfi;
			u64 Offset = FSTOffset + (i * 0x10);
			sfi.m_Unknown = Read32(Offset + 0xC);
			sfi.m_NameOffset = (u64)Read32(Offset + 0x0);
			sfi.m_Offset = ((u64)Read32(Offset + 0x4) << 5) + (header[sfi.m_Unknown & 0xFF].StartingCluster * 0x8000);
			sfi.m_FileSize   = Read32(Offset + 0x8);

			m_FileInfoVector.push_back(sfi);
		}

		u64 NameTableOffset = FSTOffset + Root.m_FileSize * 0x10;
		BuildFilenames(1, m_FileInfoVector.size(), "", NameTableOffset);
		delete[] header;
	}
}

size_t CFileSystemWiiU::BuildFilenames(const size_t _FirstIndex, const size_t _LastIndex, const std::string& _szDirectory, u64 _NameTableOffset)
{
	size_t CurrentIndex = _FirstIndex;

	while (CurrentIndex < _LastIndex)
	{
		SFileInfo *rFileInfo = &m_FileInfoVector[CurrentIndex];
		u64 uOffset = _NameTableOffset + (rFileInfo->m_NameOffset & 0xFFFFFF);
		std::string filename = GetStringFromOffset(uOffset);

		// check next index
		if (rFileInfo->IsDirectory())
		{
			if (_szDirectory.empty())
				rFileInfo->m_FullPath += StringFromFormat("%s/", filename.c_str());
			else
				rFileInfo->m_FullPath += StringFromFormat("%s%s/", _szDirectory.c_str(), filename.c_str());

			CurrentIndex = BuildFilenames(CurrentIndex + 1, (size_t) rFileInfo->m_FileSize, rFileInfo->m_FullPath, _NameTableOffset);
		}
		else // This is a filename
		{
			if (_szDirectory.empty())
				rFileInfo->m_FullPath += filename;
			else
				rFileInfo->m_FullPath += StringFromFormat("%s%s", _szDirectory.c_str(), filename.c_str());

			CurrentIndex++;
		}
	}

	return CurrentIndex;
}

}  // namespace
