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

void onRoundStart(IGameEvent &) {
  Extras::get().resetStreak();
  Extras::get().stopMvp();
}

void onRoundMvp(IGameEvent &Event) {
  if (Event.isLocalPlayer("userid")) {
    Extras::get().onMvp();
  }
}

GameEventHandler DeathHandler{"player_death", onPlayerDeath};
GameEventHandler RoundHandler{"round_start", onRoundStart};
GameEventHandler MvpHandler{"round_mvp", onRoundMvp};

} // namespace

} // namespace oc::extras
