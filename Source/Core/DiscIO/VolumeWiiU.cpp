// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <string>
#include <vector>

#include "Common/Common.h"
#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/FileMonitor.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWiiU.h"

namespace DiscIO
{
CVolumeWiiU::CVolumeWiiU(IBlobReader* _pReader)
	: m_pReader(_pReader)
{}

CVolumeWiiU::~CVolumeWiiU()
{
	delete m_pReader;
	m_pReader = nullptr; // I don't think this makes any difference, but anyway
}

bool CVolumeWiiU::Read(u64 _Offset, u64 _Length, u8* _pBuffer, bool decrypt) const
{
	if (m_pReader == nullptr)
		return false;

	FileMon::FindFilename(_Offset);

	return m_pReader->Read(_Offset, _Length, _pBuffer);
}

bool CVolumeWiiU::RAWRead(u64 _Offset, u64 _Length, u8* _pBuffer) const
{
	return Read(_Offset, _Length, _pBuffer);
}

std::string CVolumeWiiU::GetUniqueID() const
{
	static const std::string NO_UID("NO_UID");
	if (m_pReader == nullptr)
		return NO_UID;

	char ID[7];

	if (!Read(6, sizeof(ID), reinterpret_cast<u8*>(ID)))
	{
		PanicAlertT("Failed to read unique ID from disc image");
		return NO_UID;
	}

	ID[4] = ID[5];
	ID[5] = ID[6];
	ID[6] = '\0';
	return ID;
}

std::string CVolumeWiiU::GetRevisionSpecificUniqueID() const
{
	return GetUniqueID() + StringFromFormat("r%d", GetRevision());
}

IVolume::ECountry CVolumeWiiU::GetCountry() const
{
	if (!m_pReader)
		return COUNTRY_UNKNOWN;

	u8 CountryCode;
	m_pReader->Read(9, 1, &CountryCode);

	return CountrySwitch(CountryCode);
}

std::string CVolumeWiiU::GetMakerID() const
{
	return std::string();
}

int CVolumeWiiU::GetRevision() const
{
	return 0;
}

std::vector<std::string> CVolumeWiiU::GetNames() const
{
	std::vector<std::string> names;

	if (m_pReader == nullptr)
		return names;

	char ID[23];

	if (!Read(0, sizeof(ID), reinterpret_cast<u8*>(ID)))
	{
		PanicAlertT("Failed to read Wii U game name from disc image");
		ID[0] = '\0';
	}

	ID[22] = '\0';

	auto const string_decoder = GetStringDecoder(GetCountry());

	names.push_back(string_decoder(ID));

	return names;
}

u32 CVolumeWiiU::GetFSTSize() const
{
	return 0;
}

std::string CVolumeWiiU::GetApploaderDate() const
{
	return std::string();
}

u64 CVolumeWiiU::GetSize() const
{
	if (m_pReader)
		return m_pReader->GetDataSize();
	else
		return 0;
}

u64 CVolumeWiiU::GetRawSize() const
{
	if (m_pReader)
		return m_pReader->GetRawSize();
	else
		return 0;
}

bool CVolumeWiiU::IsDiscTwo() const
{
	return false;
}

auto CVolumeWiiU::GetStringDecoder(ECountry country) -> StringDecoder
{
	return (COUNTRY_JAPAN == country || COUNTRY_TAIWAN == country) ?
		SHIFTJISToUTF8 : CP1252ToUTF8;
}

} // namespace
