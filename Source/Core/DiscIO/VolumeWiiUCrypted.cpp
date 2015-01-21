// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <polarssl/aes.h>
#include <polarssl/sha1.h>

#include "Common/Common.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeGC.h"
#include "DiscIO/VolumeWiiUCrypted.h"

namespace DiscIO
{

CVolumeWiiUCrypted::CVolumeWiiUCrypted(IBlobReader* _pReader, u64 _VolumeOffset,
									 const unsigned char* _pDiscKey, const unsigned char* _pCommonKey)
	: m_pReader(_pReader),
	m_AES_ctx(new aes_context),
	m_pBuffer(nullptr),
	m_VolumeOffset(_VolumeOffset),
	dataOffset(0),
	m_LastDecryptedBlockOffset(-1),
	m_useTitleKey(false)
{
	memcpy(m_pDiscKey, _pDiscKey, 16);
	memcpy(m_pCommonKey, _pCommonKey, 16);
	if (m_useTitleKey)
		aes_setkey_dec(m_AES_ctx.get(), m_pTitleKey, 128);
	else
		aes_setkey_dec(m_AES_ctx.get(), m_pDiscKey, 128);
	m_pBuffer = new u8[0x8000];
}

void CVolumeWiiUCrypted::SetTitleKey(u8 *key)
{
	memcpy(m_pTitleKey, key, 16);
}

void CVolumeWiiUCrypted::UseTitleKey(bool _use)
{
	m_useTitleKey = _use;
}

bool CVolumeWiiUCrypted::ChangePartition(u64 offset)
{
	m_VolumeOffset = offset;
	m_LastDecryptedBlockOffset = -1;

	//DiscIO::VolumeKeyForParition(*m_pReader, offset, volume_key);
	if (m_useTitleKey)
		aes_setkey_dec(m_AES_ctx.get(), m_pTitleKey, 128);
	else
		aes_setkey_dec(m_AES_ctx.get(), m_pDiscKey, 128);
	return true;
}

CVolumeWiiUCrypted::~CVolumeWiiUCrypted()
{
	delete[] m_pBuffer;
	m_pBuffer = nullptr;
}

bool CVolumeWiiUCrypted::RAWRead( u64 _Offset, u64 _Length, u8* _pBuffer ) const
{
	if (!m_pReader->Read(_Offset, _Length, _pBuffer))
	{
		return(false);
	}
	return true;
}

bool CVolumeWiiUCrypted::Read(u64 _ReadOffset, u64 _Length, u8* _pBuffer, bool decrypt) const
{
	if (m_pReader == nullptr)
	{
		return(false);
	}
	if (!decrypt && _ReadOffset == 0)
	{
		return RAWRead(_ReadOffset, _Length, _pBuffer);
	}

	// The first cluster of a partition is unencrypted
	if (_ReadOffset < 0x8000)
		return RAWRead(_ReadOffset + m_VolumeOffset + dataOffset, _Length, _pBuffer);

	while (_Length > 0)
	{

		// math block offset
		u64 Block  = _ReadOffset / 0x8000;
		u64 Offset = _ReadOffset % 0x8000;

		// read current block
		if (!m_pReader->Read(m_VolumeOffset + dataOffset + Block * 0x8000, 0x8000, m_pBuffer))
		{
			return(false);
		}

		if (m_LastDecryptedBlockOffset != Block)
		{
			u8 IV[16] = { 0 };
			aes_crypt_cbc(m_AES_ctx.get(), AES_DECRYPT, 0x8000, IV, m_pBuffer, m_LastDecryptedBlock);

			m_LastDecryptedBlockOffset = Block;
		}

		// copy the encrypted data
		u64 MaxSizeToCopy = 0x8000 - Offset;
		u64 CopySize = (_Length > MaxSizeToCopy) ? MaxSizeToCopy : _Length;
		memcpy(_pBuffer, &m_LastDecryptedBlock[Offset], (size_t)CopySize);

		// increase buffers
		_Length -= CopySize;
		_pBuffer    += CopySize;
		_ReadOffset += CopySize;
	}

	return(true);
}

bool CVolumeWiiUCrypted::GetTitleID(u8* _pBuffer) const
{
	// Tik is at ?
	// Title Key offset in tik is 0x1BF
	return false;
}

std::string CVolumeWiiUCrypted::GetUniqueID() const
{
	static const std::string NO_UID("NO_UID");
	if (m_pReader == nullptr)
		return NO_UID;

	char ID[7];

	if (!RAWRead(6, sizeof(ID), reinterpret_cast<u8*>(ID)))
	{
		PanicAlertT("Failed to read unique ID from disc image");
		return NO_UID;
	}

	ID[4] = ID[5];
	ID[5] = ID[6];
	ID[6] = '\0';
	return ID;
}


IVolume::ECountry CVolumeWiiUCrypted::GetCountry() const
{
	if (!m_pReader)
		return COUNTRY_UNKNOWN;

	u8 CountryCode;
	m_pReader->Read(9, 1, &CountryCode);

	return CountrySwitch(CountryCode);
}

std::string CVolumeWiiUCrypted::GetMakerID() const
{
	return std::string();
}

std::vector<std::string> CVolumeWiiUCrypted::GetNames() const
{
	std::vector<std::string> names;

	if (m_pReader == nullptr)
		return names;

	char ID[23];

	if (!RAWRead(0, sizeof(ID), reinterpret_cast<u8*>(ID)))
	{
		PanicAlertT("Failed to read Wii U game name from disc image");
		ID[0] = '\0';
	}

	ID[22] = '\0';

	auto const string_decoder = CVolumeGC::GetStringDecoder(GetCountry());

	names.push_back(string_decoder(ID));

	return names;
}

u32 CVolumeWiiUCrypted::GetFSTSize() const
{
	return 0;
}

std::string CVolumeWiiUCrypted::GetApploaderDate() const
{
	return std::string();
}

u64 CVolumeWiiUCrypted::GetSize() const
{
	if (m_pReader)
	{
		return m_pReader->GetDataSize();
	}
	else
	{
		return 0;
	}
}

u64 CVolumeWiiUCrypted::GetRawSize() const
{
	if (m_pReader)
	{
		return m_pReader->GetRawSize();
	}
	else
	{
		return 0;
	}
}

bool CVolumeWiiUCrypted::CheckIntegrity() const
{
	return true;
}

} // namespace
