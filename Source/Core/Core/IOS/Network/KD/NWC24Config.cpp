// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Network/KD/NWC24Config.h"

#include <cstring>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

namespace IOS
{
namespace HLE
{
namespace NWC24
{
NWC24Config::NWC24Config()
{
  m_path = File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/" WII_WC24CONF_DIR "/nwc24msg.cfg";
  ReadConfig();
}

void NWC24Config::ReadConfig()
{
  if (!File::IOFile(m_path, "rb").ReadBytes(&m_data, sizeof(m_data)))
  {
    ResetConfig();
  }
  else
  {
    const s32 config_error = CheckNwc24Config();
    if (config_error)
      ERROR_LOG(IOS_WC24, "There is an error in the config for for WC24: %d", config_error);
  }
}

void NWC24Config::WriteConfig() const
{
  if (!File::Exists(m_path))
  {
    if (!File::CreateFullPath(File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/" WII_WC24CONF_DIR))
    {
      ERROR_LOG(IOS_WC24, "Failed to create directory for WC24");
    }
  }

  File::IOFile(m_path, "wb").WriteBytes(&m_data, sizeof(m_data));
}

void NWC24Config::ResetConfig()
{
  if (File::Exists(m_path))
    File::Delete(m_path);
#if defined(_MSC_VER) && _MSC_VER <= 1800
#define constexpr
#endif
  constexpr const char* urls[5] = {
      "https://amw.wc24.wii.com/cgi-bin/account.cgi", "http://rcw.wc24.wii.com/cgi-bin/check.cgi",
      "http://mtw.wc24.wii.com/cgi-bin/receive.cgi",  "http://mtw.wc24.wii.com/cgi-bin/delete.cgi",
      "http://mtw.wc24.wii.com/cgi-bin/send.cgi",
  };

  memset(&m_data, 0, sizeof(m_data));

  SetMagic(0x57634366);
  SetUnk(8);
  SetCreationStage(NWC24_IDCS_INITIAL);
  SetEnableBooting(0);
  SetEmail("@wii.com");

  for (int i = 0; i < URL_COUNT; ++i)
  {
    strncpy(m_data.http_urls[i], urls[i], MAX_URL_LENGTH);
  }

  SetChecksum(CalculateNwc24ConfigChecksum());

  WriteConfig();
}

u32 NWC24Config::CalculateNwc24ConfigChecksum() const
{
  const u32* ptr = reinterpret_cast<const u32*>(&m_data);
  u32 sum = 0;

  for (int i = 0; i < 0xFF; ++i)
  {
    sum += Common::swap32(*ptr++);
  }

  return sum;
}

s32 NWC24Config::CheckNwc24Config() const
{
  // 'WcCf' magic
  if (Magic() != 0x57634366)
  {
    ERROR_LOG(IOS_WC24, "Magic mismatch");
    return -14;
  }

  const u32 checksum = CalculateNwc24ConfigChecksum();
  DEBUG_LOG(IOS_WC24, "Checksum: %X", checksum);
  if (Checksum() != checksum)
  {
    ERROR_LOG(IOS_WC24, "Checksum mismatch expected %X and got %X", checksum, Checksum());
    return -14;
  }

  if (IdGen() > 0x1F)
  {
    ERROR_LOG(IOS_WC24, "Id gen error");
    return -14;
  }

  if (Unk() != 8)
    return -27;

  return 0;
}

u32 NWC24Config::Magic() const
{
  return Common::swap32(m_data.magic);
}

void NWC24Config::SetMagic(u32 magic)
{
  m_data.magic = Common::swap32(magic);
}

u32 NWC24Config::Unk() const
{
  return Common::swap32(m_data.unk_04);
}

void NWC24Config::SetUnk(u32 unk_04)
{
  m_data.unk_04 = Common::swap32(unk_04);
}

u32 NWC24Config::IdGen() const
{
  return Common::swap32(m_data.id_generation);
}

void NWC24Config::SetIdGen(u32 id_generation)
{
  m_data.id_generation = Common::swap32(id_generation);
}

void NWC24Config::IncrementIdGen()
{
  u32 id_ctr = IdGen();
  id_ctr++;
  id_ctr &= 0x1F;

  SetIdGen(id_ctr);
}

u32 NWC24Config::Checksum() const
{
  return Common::swap32(m_data.checksum);
}

void NWC24Config::SetChecksum(u32 checksum)
{
  m_data.checksum = Common::swap32(checksum);
}

u32 NWC24Config::CreationStage() const
{
  return Common::swap32(m_data.creation_stage);
}

void NWC24Config::SetCreationStage(u32 creation_stage)
{
  m_data.creation_stage = Common::swap32(creation_stage);
}

u32 NWC24Config::EnableBooting() const
{
  return Common::swap32(m_data.enable_booting);
}

void NWC24Config::SetEnableBooting(u32 enable_booting)
{
  m_data.enable_booting = Common::swap32(enable_booting);
}

u64 NWC24Config::Id() const
{
  return Common::swap64(m_data.nwc24_id);
}

void NWC24Config::SetId(u64 nwc24_id)
{
  m_data.nwc24_id = Common::swap64(nwc24_id);
}

const char* NWC24Config::Email() const
{
  return m_data.email;
}

void NWC24Config::SetEmail(const char* email)
{
  strncpy(m_data.email, email, MAX_EMAIL_LENGTH);
  m_data.email[MAX_EMAIL_LENGTH - 1] = '\0';
}
}  // namespace NWC24
}  // namespace HLE
}  // namespace IOS
