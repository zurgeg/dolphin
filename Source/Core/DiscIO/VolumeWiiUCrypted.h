// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include <polarssl/aes.h>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume.h"

// --- this volume type is used for encrypted Wii U images ---

namespace DiscIO
{

class IBlobReader;

class CVolumeWiiUCrypted : public IVolume
{
public:
	CVolumeWiiUCrypted(IBlobReader* _pReader, u64 _VolumeOffset, const unsigned char* _pVolumeKey);
	~CVolumeWiiUCrypted();
	bool Read(u64 _Offset, u64 _Length, u8* _pBuffer) const override;
	bool RAWRead(u64 _Offset, u64 _Length, u8* _pBuffer) const override;
	bool GetTitleID(u8* _pBuffer) const override;
	void GetTMD(u8* _pBuffer, u32* _sz) const override;
	std::string GetUniqueID() const override;
	std::string GetMakerID() const override;
	std::vector<std::string> GetNames() const override;
	u32 GetFSTSize() const override;
	std::string GetApploaderDate() const override;
	ECountry GetCountry() const override;
	u64 GetSize() const override;
	u64 GetRawSize() const override;

	bool SupportsIntegrityCheck() const override { return true; }
	bool CheckIntegrity() const override;

private:
	IBlobReader* m_pReader;

	u8* m_pBuffer;
	aes_context* m_AES_ctx;

	u64 m_VolumeOffset;
	u64 dataOffset;

	mutable u64 m_LastDecryptedBlockOffset;
	mutable unsigned char m_LastDecryptedBlock[0x8000];
};

} // namespace
