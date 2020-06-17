// Copyright 2020 Dolphin VR Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VirtualBoy/VolumeRom.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <mbedtls/aes.h>
#include <mbedtls/sha1.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/IOS/IOSC.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/WiiSaveBanner.h"

namespace DiscIO
{
VolumeRom::VolumeRom(std::unique_ptr<BlobReader> reader) : m_reader(std::move(reader))
{
  ASSERT(m_reader);
  // Source: https://www.planetvb.com/content/downloads/documents/stsvb.html#cartridgesandromformat
}

bool VolumeRom::Read(u64 offset, u64 length, u8* buffer, const Partition& partition) const
{
  if (partition != PARTITION_NONE)
    return false;

  return m_reader->Read(offset, length, buffer);
}

const FileSystem* VolumeRom::GetFileSystem(const Partition& partition) const
{
  return nullptr;
}

Region VolumeRom::GetRegion() const
{
  char code = m_reader->ReadSwapped<char>(GetSize() - 0x220 + 0x1B + 3).value_or('\0');
  if (code == 'M' || code == 'N' || code == 'C' || code == 'O' || code == 'R' || code == 'G' ||
      code == 'X' || code == (char)0xFF)
    return Region::Unknown;
  return CountryCodeToRegion(code, GetVolumeType(), Region::NTSC_U);
}

Country VolumeRom::GetCountry(const Partition& partition) const
{
  char code = m_reader->ReadSwapped<char>(GetSize() - 0x220 + 0x1B + 3).value_or('\0');
  // Homebrew
  if (code == 'M' || code == 'N' || code == 'C' || code == 'O' || code == 'R' || code == 'G' ||
      code == 'X' || code == (char)0xFF)
    return Country::World;
  switch (m_reader->ReadSwapped<u32>(GetSize() - 0x220 + 0x1B).value_or(0))
  {
  case 'VWCJ':  // Wario Land
  case 'VTBJ':  // Teleroboxer
  case 'VGPJ':  // Galactic Pinball
  case 'VMTJ':  // Mario's Tennis
  case 'VMCJ':  // Mario Clash
    return Country::World;
  // case 'VIMJ':  // Innsmouth Mansion (technically a Japan exclusive, but in English)
  //  return Country::World;
  case 'VTRJ':
    if (GetSize() == 1024 * 1024)  // Faceball / Niko-chan Battle
      return Country::World;
    break;
  }

  return CountryCodeToCountry(code, GetVolumeType(), Region::NTSC_U);
}

std::string VolumeRom::GetGameID(const Partition& partition) const
{
  std::string id;
  id.resize(6);
  u64 header_offset = GetSize() - 0x220;
  if (!m_reader->Read(header_offset + 0x1B, 4, (u8*)id.data()))
    return "V000";
  if (!m_reader->Read(header_offset + 0x19, 2, (u8*)id.data() + 4))
    id.resize(4);
  // Galactic Pinball (VGPJ01) has a prototype called Space Pinball.
  // I'm giving it a similar ID (VSPJ was taken) but region code M (used by multilanguage homebrew)
  // and Maker ID of 00
  if (*((u32*)id.data()) == (u32)0xFFFFFFFF)
    return "VGPM00";
  // used as a filename, so don't include wildcards
  for (int i = 0; i < id.length(); i++)
    if (id[i] == '?' || id[i] == '*')
      id[i] = 'Q';
  return id;
}

std::string VolumeRom::GetGameTDBID(const Partition& partition) const
{
  // Technically it has no GameTDBID, because their database doesn't include Virtual Boy.
  return GetGameID(partition);
}

std::string VolumeRom::GetMakerID(const Partition& partition) const
{
  std::string temp;
  temp.resize(2);
  u64 header_offset = GetSize() - 0x220;
  if (!m_reader->Read(header_offset + 0x19, 2, (u8*)temp.data()))
    return "00";
  // Space Pinball
  if (!IsPrintableCharacter(temp[0]) || !IsPrintableCharacter(temp[1]))
    return "00";
  return temp;
}

std::optional<u16> VolumeRom::GetRevision(const Partition& partition) const
{
  std::optional<u16> revision = m_reader->ReadSwapped<u8>(GetSize() - 0x220 + 0x1F);
  // Space Pinball
  if (revision == (u8)0xFF)
    revision = 0;
  return revision;
}

std::string VolumeRom::GetInternalName(const Partition& partition) const
{
  char name[20];
  if (Read(GetSize() - 0x220 + 0, sizeof(name), reinterpret_cast<u8*>(name), partition))
  {
    for (int i = 0; i < sizeof(name); i++)
    {
      if (name[i] != (char)0xFF)
        return DecodeString(name);
    }
  }

  return "";
}

Platform VolumeRom::GetVolumeType() const
{
  return Platform::VirtualBoyRom;
}

std::map<Language, std::string> VolumeRom::GetLongNames() const
{
  std::map<Language, std::string> results;
  if (GetRegion() == Region::NTSC_J)
    results[Language::Japanese] = GetInternalName();
  else
    results[Language::English] = GetInternalName();
  switch (m_reader->ReadSwapped<u32>(GetSize() - 0x220 + 0x1B).value_or(0))
  {
  case 0xFFFFFFFF:
    results.clear();
    results[Language::English] = "Space Pinball (prototype of Galactic Pinball)";
    break;
  case 'VWCE':
  case 'VWCJ':
    results[Language::English] = "Virtual Boy Wario Land";
    break;
  case 'VSDJ':
    results[Language::English] = "SD Gundam: Dimension War";
    break;
  case 'VIMJ':
    results[Language::English] = "Innsmouth Mansion";
    break;
  case 'VH2E':
  case 'VH2J':
    results[Language::English] = "Panic Bomber";
    break;
  case 'VJVJ':
    results[Language::English] = "Virtual Lab";
    break;
  case 'VVPJ':
    results[Language::English] = "Virtual League Baseball";
    break;
  case 'VPBE':
    results[Language::English] = "3-D Tetris";
    break;
  case 'VREE':
  case 'VREJ':
    results[Language::English] = "Red Alarm";
    break;
  case 'VBHE':
    results[Language::English] = "Bound High!";
    break;
  case 'VNFE':
    results[Language::English] = "Nester's Funky Bowling";
    break;
  case 'VVGJ':
    results[Language::English] = "T&E Virtual Golf";
    break;
  case 'VTRJ':
    if (GetSize() == 1024 * 1024)
    {
      results.clear();
      results[Language::English] = "Faceball";
    }
    else
      results[Language::English] = "V-Tetris";
    break;
  }

  return results;
}

std::vector<u32> VolumeRom::GetBanner(u32* width, u32* height) const
{
  return {};
}

BlobType VolumeRom::GetBlobType() const
{
  return m_reader->GetBlobType();
}

u64 VolumeRom::GetSize() const
{
  return m_reader->GetDataSize();
}

bool VolumeRom::IsSizeAccurate() const
{
  return m_reader->IsDataSizeAccurate();
}

u64 VolumeRom::GetRawSize() const
{
  return m_reader->GetRawSize();
}

const BlobReader& VolumeRom::GetBlobReader() const
{
  return *m_reader;
}

}  // namespace DiscIO
