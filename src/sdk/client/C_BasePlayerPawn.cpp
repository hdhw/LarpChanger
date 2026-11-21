#include "sdk/client/C_BasePlayerPawn.h"

#include "sdk/client/CCSPlayerController.h"
#include "sdk/client/CGameEntitySystem.h"

#include <cstdint>

bool C_BasePlayerPawn::hasLocalController() {
  const std::uint32_t Index = CCSPlayerController::getLocalIndex();
  if (Index != 0 &&
      (m_hController().m_Handle & CGameEntitySystem::IndexMask) == Index) {
    return true;
  }
  // The local pawn's m_hController arrives unset (0xffffffff), so also match from the controller's side.
  CCSPlayerController *const Local = CCSPlayerController::getLocal();
  const std::uint32_t Mine = getHandle();
  return Local != nullptr && Mine != CGameEntitySystem::InvalidHandle &&
         Local->m_hPlayerPawn().m_Handle == Mine;
}
