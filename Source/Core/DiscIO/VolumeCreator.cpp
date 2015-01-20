// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <polarssl/aes.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DiscIO/VolumeDirectory.h"
#include "DiscIO/VolumeGC.h"
#include "DiscIO/VolumeWad.h"
#include "DiscIO/VolumeWiiCrypted.h"
#include "DiscIO/VolumeWiiU.h"
#include "DiscIO/VolumeWiiUCrypted.h"


namespace DiscIO
{
enum EDiscType
{
	DISC_TYPE_UNK,
	DISC_TYPE_WII,
	DISC_TYPE_WII_CONTAINER,
	DISC_TYPE_GC,
	DISC_TYPE_WAD,
	DISC_TYPE_WIIU
};

class CBlobBigEndianReader
{
public:
	CBlobBigEndianReader(IBlobReader& _rReader) : m_rReader(_rReader) {}

	u32 Read32(u64 _Offset)
	{
		u32 Temp;
		m_rReader.Read(_Offset, 4, (u8*)&Temp);
		return Common::swap32(Temp);
	}
	u16 Read16(u64 _Offset)
	{
		u16 Temp;
		m_rReader.Read(_Offset, 2, (u8*)&Temp);
		return Common::swap16(Temp);
	}
	u8 Read8(u64 _Offset)
	{
		u8 Temp;
		m_rReader.Read(_Offset, 1, &Temp);
		return Temp;
	}
private:
	IBlobReader& m_rReader;
};

static const unsigned char s_master_key[16] = {
	0xeb,0xe4,0x2a,0x22,0x5e,0x85,0x93,0xe4,
	0x48,0xd9,0xc5,0x45,0x73,0x81,0xaa,0xf7
};

static const unsigned char s_master_key_korean[16] = {
	0x63,0xb8,0x2b,0xb4,0xf4,0x61,0x4e,0x2e,
	0x13,0xf2,0xfe,0xfb,0xba,0x4c,0x9b,0x7e
};

static const unsigned char s_master_key_wiiu[16] = {
	0xD7,0xB0,0x04,0x02,0x65,0x9B,0xA2,0xAB,
	0xD2,0xCB,0x0D,0xB2,0x7F,0xA2,0xB6,0x56
};

static IVolume* CreateVolumeFromCryptedWiiImage(IBlobReader& _rReader, u32 _PartitionGroup, u32 _VolumeType, u32 _VolumeNum);
static IVolume* CreateVolumeFromCryptedWiiUImage(IBlobReader& _rReader, u32 _PartitionGroup, u32 _VolumeType, u32 _VolumeNum, bool Korean);
EDiscType GetDiscType(IBlobReader& _rReader);

IVolume* CreateVolumeFromFilename(const std::string& _rFilename, u32 _PartitionGroup, u32 _VolumeNum)
{
	IBlobReader* pReader = CreateBlobReader(_rFilename);
	if (pReader == nullptr)
		return nullptr;

	switch (GetDiscType(*pReader))
	{
		case DISC_TYPE_WII:
		case DISC_TYPE_GC:
			return new CVolumeGC(pReader);

		case DISC_TYPE_WAD:
			return new CVolumeWAD(pReader);

		case DISC_TYPE_WIIU:
		{
			IVolume* pVolume = CreateVolumeFromCryptedWiiUImage(*pReader, _PartitionGroup, 0, _VolumeNum, false);

			if (pVolume == nullptr)
			{
				delete pReader;
			}

			return pVolume;
		}
		break;

		case DISC_TYPE_WII_CONTAINER:
		{
			IVolume* pVolume = CreateVolumeFromCryptedWiiImage(*pReader, _PartitionGroup, 0, _VolumeNum);

			if (pVolume == nullptr)
			{
				delete pReader;
			}

			return pVolume;
		}

		case DISC_TYPE_UNK:
		default:
			std::string Filename, ext;
			SplitPath(_rFilename, nullptr, &Filename, &ext);
			Filename += ext;
			NOTICE_LOG(DISCIO, "%s does not have the Magic word for a gcm, wiidisc or wad file\n"
						"Set Log Verbosity to Warning and attempt to load the game again to view the values", Filename.c_str());
			delete pReader;
	}

	return nullptr;
}

IVolume* CreateVolumeFromDirectory(const std::string& _rDirectory, bool _bIsWii, const std::string& _rApploader, const std::string& _rDOL)
{
	if (CVolumeDirectory::IsValidDirectory(_rDirectory))
		return new CVolumeDirectory(_rDirectory, _bIsWii, _rApploader, _rDOL);

	return nullptr;
}

bool IsVolumeWiiDisc(const IVolume *_rVolume)
{
	u32 MagicWord = 0;
	_rVolume->Read(0x18, 4, (u8*)&MagicWord, false);

	return (Common::swap32(MagicWord) == 0x5D1C9EA3);
	//GameCube 0xc2339f3d
}

bool IsVolumeWiiUDisc(const IVolume *_rVolume)
{
	u32 MagicWord = 0;
	_rVolume->Read(0x0, 4, (u8*)&MagicWord, false);

	return (Common::swap32(MagicWord) == 0x5755502D);	
}

bool IsVolumeWadFile(const IVolume *_rVolume)
{
	u32 MagicWord = 0;
	_rVolume->Read(0x02, 4, (u8*)&MagicWord, false);

	return (Common::swap32(MagicWord) == 0x00204973 || Common::swap32(MagicWord) == 0x00206962);
}

void VolumeKeyForParition(IBlobReader& _rReader, u64 offset, u8* VolumeKey)
{
	CBlobBigEndianReader Reader(_rReader);

	u8 SubKey[16];
	_rReader.Read(offset + 0x1bf, 16, SubKey);

	u8 IV[16];
	memset(IV, 0, 16);
	_rReader.Read(offset + 0x44c, 8, IV);

	bool usingKoreanKey = false;
	// Issue: 6813
	// Magic value is at partition's offset + 0x1f1 (1byte)
	// If encrypted with the Korean key, the magic value would be 1
	// Otherwise it is zero
	if (Reader.Read8(0x3) == 'K' && Reader.Read8(offset + 0x1f1) == 1)
		usingKoreanKey = true;

	aes_context AES_ctx;
	aes_setkey_dec(&AES_ctx, (usingKoreanKey ? s_master_key_korean : s_master_key), 128);

	aes_crypt_cbc(&AES_ctx, AES_DECRYPT, 16, IV, SubKey, VolumeKey);
}

static IVolume* CreateVolumeFromCryptedWiiImage(IBlobReader& _rReader, u32 _PartitionGroup, u32 _VolumeType, u32 _VolumeNum)
{
	CBlobBigEndianReader Reader(_rReader);

	u32 numPartitions = Reader.Read32(0x40000 + (_PartitionGroup * 8));
	u64 PartitionsOffset = (u64)Reader.Read32(0x40000 + (_PartitionGroup * 8) + 4) << 2;

	// Check if we're looking for a valid partition
	if ((int)_VolumeNum != -1 && _VolumeNum > numPartitions)
		return nullptr;

	struct SPartition
	{
		u64 Offset;
		u32 Type;
	};

	struct SPartitionGroup
	{
		u32 numPartitions;
		u64 PartitionsOffset;
		std::vector<SPartition> PartitionsVec;
	};
	SPartitionGroup PartitionGroup[4];

	// read all partitions
	for (SPartitionGroup& group : PartitionGroup)
	{
		for (u32 i = 0; i < numPartitions; i++)
		{
			SPartition Partition;
			Partition.Offset = ((u64)Reader.Read32(PartitionsOffset + (i * 8) + 0)) << 2;
			Partition.Type   = Reader.Read32(PartitionsOffset + (i * 8) + 4);
			group.PartitionsVec.push_back(Partition);
		}
	}

	// return the partition type specified or number
	// types: 0 = game, 1 = firmware update, 2 = channel installer
	//  some partitions on ssbb use the ascii title id of the demo VC game they hold...
	for (size_t i = 0; i < PartitionGroup[_PartitionGroup].PartitionsVec.size(); i++)
	{
		const SPartition& rPartition = PartitionGroup[_PartitionGroup].PartitionsVec.at(i);

		if ((rPartition.Type == _VolumeType && (int)_VolumeNum == -1) || i == _VolumeNum)
		{
			u8 VolumeKey[16];
			VolumeKeyForParition(_rReader, rPartition.Offset, VolumeKey);
			return new CVolumeWiiCrypted(&_rReader, rPartition.Offset, VolumeKey);
		}
	}

	return nullptr;
}

static u32 PointerRead16(u8 *p)
{
	return Common::swap16(*((u16*)p));
}

static u32 PointerRead32(u8 *p)
{
	return Common::swap32(*((u32*)p));
}

#define GAMECODE(a, b, c, d) ((u32)(a) << 24 | ((u32)(b) << 16) | ((u32)(c) << 8) | ((u32)(d) & 0xFF))

static IVolume* CreateVolumeFromCryptedWiiUImage(IBlobReader& _rReader, u32 _PartitionGroup, u32 _VolumeType, u32 _VolumeNum, bool Korean)
{
	CBlobBigEndianReader Reader(_rReader);
	// Read game ID
	u32 game_id = Reader.Read32(0x06);

	// Get title key for that game
	const u8 *title_key;
	const u8 SM3DWKey[16] = { 0xE2, 0x3A, 0xEA, 0x15, 0x4F, 0x14, 0x28, 0x15, 0x6D, 0x25, 0xBF, 0xCC, 0x40, 0xF6, 0x38, 0x56 };
	const u8 NintendoLandKey[16] = { 0xB0, 0xD8, 0x49, 0x1C, 0x8B, 0x98, 0x35, 0xDC, 0x98, 0x05, 0x77, 0x23, 0xED, 0x22, 0x00, 0xCA };
	const u8 EspnKey[16] = { 0x31, 0xf9, 0x87, 0x63, 0x6f, 0xfe, 0x9d, 0xcd, 0x90, 0x9c, 0xf6, 0xab, 0x86, 0x15, 0xf9, 0x79 };
	const u8 TankKey[16] = { 0x6e, 0xff, 0x58, 0x91, 0x14, 0xdc, 0xc0, 0x7e, 0x9a, 0xa4, 0xca, 0x94, 0x17, 0xb0, 0xaa, 0x30 };
	const u8 SonicKey[16] = { 0x99, 0xbc, 0x84, 0xdb, 0x36, 0x30, 0x31, 0x55, 0xe7, 0x0b, 0x5c, 0x98, 0x69, 0xce, 0x4e, 0x86 };
	const u8 WarioKey[16] = { 0x69, 0x4a, 0x02, 0x9c, 0x58, 0x59, 0x08, 0xe2, 0xee, 0x50, 0xd1, 0xdd, 0x31, 0x25, 0x37, 0xad };
	const u8 PikminKey[16] = { 0x2a, 0xde, 0xcd, 0xd1, 0x54, 0xfb, 0xfe, 0x2c, 0x2e, 0x56, 0xef, 0x27, 0xf8, 0x34, 0x47, 0x96 };
	const u8 NSMBKey[16] = { 0x18, 0x5f, 0x9d, 0x54, 0xd9, 0x85, 0x99, 0xab, 0x5f, 0xc4, 0xac, 0xec, 0x76, 0xe8, 0x66, 0x45 };
	const u8 DuckTalesKey[16] = { 0x85, 0xde, 0x1b, 0x56, 0x16, 0x6d, 0x1c, 0x02, 0x97, 0x5c, 0x6c, 0xd1, 0x8d, 0x86, 0x0e, 0x6e };
	const u8 MarioKartKey[16] = { 0xc3, 0xf8, 0x73, 0xc4, 0xe0, 0x1e, 0xa0, 0x28, 0x17, 0xe1, 0x82, 0x89, 0x8e, 0xce, 0xbc, 0x74 };
	const u8 ZeldaKey[16] = { 0xc0, 0xfe, 0x8a, 0xae, 0xe5, 0xf6, 0xe7, 0xb5, 0xb1, 0x07, 0x4a, 0x46, 0x09, 0x06, 0xa2, 0x8f };
	const u8 AvengersKey[16] = { 0x02, 0x7c, 0x95, 0x57, 0x64, 0x8a, 0x1a, 0x99, 0x9a, 0xa7, 0x84, 0x83, 0x19, 0xbb, 0x5e, 0xf2 };
	const u8 DonkeyKongKey[16] = { 0x77, 0xf5, 0x14, 0x31, 0x74, 0x69, 0x83, 0xae, 0xa5, 0x01, 0xd1, 0xea, 0xcb, 0x8d, 0xaf, 0x54 };
	const u8 SochiKey[16] = { 0x4d, 0x8a, 0xc3, 0x03, 0x59, 0xb9, 0x31, 0x1f, 0x06, 0x02, 0x88, 0x3a, 0x77, 0x8a, 0x6d, 0x07 };
	const u8 CallOfDutyKey[16] = { 0x28, 0x51, 0x2a, 0x78, 0x01, 0x3b, 0x12, 0x7e, 0x41, 0x8f, 0x02, 0xa1, 0xf4, 0x5e, 0xfb, 0x99 };

	switch (game_id)
	{
	case GAMECODE('A', 'C', '3', 'E'):
		title_key = PikminKey;
		break;
	case GAMECODE('A', 'C', 'P', 'E'):
		title_key = CallOfDutyKey;
		break;
	case GAMECODE('A', 'L', 'C', 'E'):
		title_key = NintendoLandKey;
		break;
	case GAMECODE('A', 'M', 'K', 'E'):
		title_key = MarioKartKey;
		break;
	case GAMECODE('A', 'M', 'V', 'P'):
		title_key = AvengersKey;
		break;
	case GAMECODE('A', 'R', 'D', 'E'):
		title_key = SM3DWKey;
		break;
	case GAMECODE('A', 'R', 'K', 'E'):
	case GAMECODE('A', 'R', 'K', 'P'):
	case GAMECODE('A', 'R', 'K', 'J'):
		title_key = DonkeyKongKey;
		break;
	case GAMECODE('A', 'R', 'P', 'E'):
		title_key = NSMBKey;
		break;
	case GAMECODE('A', 'R', 'U', 'E'):
		title_key = SochiKey;
		break;
	case GAMECODE('A', 'S', 'N', 'E'):
		title_key = SonicKey;
		break;
	// ESPN sport connection USA
	case GAMECODE('A', 'S', 'P', 'E'):
		title_key = EspnKey;
		break;
	case GAMECODE('A', 'T', 'K', 'E'):
		title_key = TankKey;
		break;
	// Wind Waker HD
	case GAMECODE('B', 'C', 'Z', 'E'):
		title_key = ZeldaKey;
		break;
	case GAMECODE('A', 'S', 'A', 'E'):
	case GAMECODE('G', 'W', 'W', 'E'):
		title_key = WarioKey;
		break;
	case GAMECODE('W', 'D', 'K', 'E'):
		title_key = DuckTalesKey;
		break;
	default:
		// We don't know the title key, so return an undecrypted volume
		if ((int)_VolumeNum != -1 && _VolumeNum >= 1)
			return nullptr;
		else
			return new CVolumeWiiU(&_rReader);
	}

	// Read cluster at 0x18000, then decrypt it using title key
	u8 encrypted[0x8000], decrypted[0x8000];
	_rReader.Read(0x18000, 0x8000, encrypted);
	aes_context AES_ctx;
	aes_setkey_dec(&AES_ctx, title_key, 128);
	u8 IV[16];
	memset(IV, 0, 16);
	aes_crypt_cbc(&AES_ctx, AES_DECRYPT, 0x8000, IV, encrypted, decrypted);

	u32 magic = PointerRead32(&decrypted[0]);
	if (magic != 0xCCA6E67B)
	{
		PanicAlertT("Couldn't load Wii U partition.");
		return nullptr;
	}
	u32 numPartitions = PointerRead32(&decrypted[0x1C]);

	// Check if we're looking for a valid partition
	if ((int)_VolumeNum != -1 && _VolumeNum >= numPartitions)
		return nullptr;

	// return the partition type specified or number
	// types: 0 = game, 1 = firmware update, 2 = channel installer
	//  some partitions on ssbb use the ascii title id of the demo VC game they hold...
	if ((int)_VolumeNum == -1)
	{
		for (int i = 0; i < (int)numPartitions; ++i)
		{
			u16 code = PointerRead16(&decrypted[0x800 + 0x80*i]);
			u32 type;
			switch (code)
			{
				// "SI", always the first partition? Assume it's a channel installer
				case 0x5349: 
					type = 2; 
					break;
				// "UP", update
				case 0x5550:
					type = 1;
					break;
				// "GM", game
				case 0x474D:
					type = 0;
					break;
				default:
					type = code;
			}
			if (type == _VolumeType)
			{
				u32 offset = 0x8000 * PointerRead32(&decrypted[0x800 + 0x80 * i + 0x20]);
				return new CVolumeWiiUCrypted(&_rReader, offset, title_key, s_master_key_wiiu);
			}
		}
		return nullptr;
	}
	u32 offset = 0x8000 * PointerRead32(&decrypted[0x800 + 0x80 * _VolumeNum + 0x20]);
	return new CVolumeWiiUCrypted(&_rReader, offset, title_key, s_master_key_wiiu);
}

EDiscType GetDiscType(IBlobReader& _rReader)
{
	CBlobBigEndianReader Reader(_rReader);
	u32 WiiMagic = Reader.Read32(0x18);
	u32 WiiContainerMagic = Reader.Read32(0x60);
	u32 WADMagic = Reader.Read32(0x02);
	u32 GCMagic = Reader.Read32(0x1C);
	u32 WiiUMagic = Reader.Read32(0x00);

	// check for Wii U ("WUP-"), all Wii U product codes begin with "WUP-", not just games.
	if (WiiUMagic == 0x5755502D)
		return DISC_TYPE_WIIU;

	// check for Wii
	if (WiiMagic == 0x5D1C9EA3 && WiiContainerMagic != 0)
		return DISC_TYPE_WII;
	if (WiiMagic == 0x5D1C9EA3 && WiiContainerMagic == 0)
		return DISC_TYPE_WII_CONTAINER;

	// check for WAD
	// 0x206962 for boot2 wads
	if (WADMagic == 0x00204973 || WADMagic == 0x00206962)
		return DISC_TYPE_WAD;

	// check for GC
	if (GCMagic == 0xC2339F3D)
		return DISC_TYPE_GC;

	WARN_LOG(DISCIO, "No known magic words found");
	WARN_LOG(DISCIO, "Wii  offset: 0x18 value: 0x%08x", WiiMagic);
	WARN_LOG(DISCIO, "WiiC offset: 0x60 value: 0x%08x", WiiContainerMagic);
	WARN_LOG(DISCIO, "WAD  offset: 0x02 value: 0x%08x", WADMagic);
	WARN_LOG(DISCIO, "GC   offset: 0x1C value: 0x%08x", GCMagic);

	return DISC_TYPE_UNK;
}

}  // namespace
