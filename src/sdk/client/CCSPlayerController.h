#pragma once

#include "sdk/client/CBasePlayerController.h"
#include "sdk/client/CHandle.h"
#include "sdk/macros.h"
#include "sdk/tier1/CUtlString.h"

#include <cstdint>

class CCSPlayerController_InventoryServices;
class C_CSPlayerPawn;

class CCSPlayerController : public CBasePlayerController {
public:
  SDK_CLASS(CCSPlayerController);

  SCHEMA_FIELD(CHandle<C_CSPlayerPawn>, m_hPlayerPawn);
  SCHEMA_FIELD(CCSPlayerController_InventoryServices *, m_pInventoryServices);
  SCHEMA_FIELD(const char *, m_szClan);
  SCHEMA_FIELD(CUtlString, m_sSanitizedClanTag);

  static std::uint32_t getLocalIndex();
  static CCSPlayerController *getLocal();
  static C_CSPlayerPawn *getLocalPawn();

  C_CSPlayerPawn *getPawn();
};
