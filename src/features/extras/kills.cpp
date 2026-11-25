#include "features/extras/Extras.h"
#include "game/GameEventHandler.h"
#include "sdk/client/IGameEvent.h"

namespace oc::extras {

namespace {

void onPlayerDeath(IGameEvent &Event) {
  const bool Died = Event.isLocalPlayer("userid");
  if (Died) {
    Extras::get().resetStreak();
  } else if (Event.isLocalPlayer("attacker")) {
    Extras::get().onKill();
  }
}

void onRoundStart(IGameEvent &) { Extras::get().resetStreak(); }

GameEventHandler DeathHandler{"player_death", onPlayerDeath};
GameEventHandler RoundHandler{"round_start", onRoundStart};

} // namespace

} // namespace oc::extras
