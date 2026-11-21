#include "core/log.h"
#include "features/changer/Changer.h"
#include "features/changer/catalog/catalog.h"
#include "features/changer/loadout/loadout.h"
#include "game/DecodedHandler.h"
#include "game/GameEventHandler.h"
#include "sdk/client/CBodyComponent.h"
#include "sdk/client/CCSInventoryManager.h"
#include "sdk/client/CCSPlayerController.h"
#include "sdk/client/CCSPlayerController_InventoryServices.h"
#include "sdk/client/CEntityInstance.h"
#include "sdk/client/CModelState.h"
#include "sdk/client/C_CSPlayerPawn.h"
#include "sdk/client/C_EconEntity.h"
#include "sdk/client/C_EconItemView.h"
#include "sdk/client/IGameEvent.h"
#include "sdk/networksystem/CNetworkFieldChangedEventQueue.h"

#include <cstdint>
#include <string>

namespace oc::changer {

namespace {

void onDecoded(CEntityInstance &Entity,
               CNetworkFieldChangedEventQueue *Changes);

DecodedHandler Handler{onDecoded};

void show(C_EconItemView &View, const ItemRecord &Record,
          const SchemaIds &Ids) {
  View.setView(Record.Definition, ourItemId(Record, Ids),
               CCSInventoryManager::get()->getLocalAccountId(),
               qualityOf(Record, Ids), Record.Name);
}

std::uint16_t defaultKnife(std::uint8_t Team, const SchemaIds &Ids) {
  if (Team != Ids.Terrorists && Team != Ids.CounterTerrorists) {
    return 0;
  }
  return CCSInventoryManager::get()
      ->getDefaultItem(Team, Ids.Knife)
      .m_iItemDefinitionIndex();
}

const ItemRecord *recordOf(const Loadout &Items, C_EconItemView &View,
                           std::uint8_t Team, const SchemaIds &Ids) {
  const std::uint16_t Definition = View.m_iItemDefinitionIndex();
  if (const ItemRecord *const Record = Items.byDefinition(Team, Definition)) {
    return Record;
  }
  const ItemRecord *const Knife = Items.find(Team, Ids.Knife, Ids);
  if (Knife != nullptr && Definition == defaultKnife(Team, Ids)) {
    return Knife;
  }
  return Items.knifeById(View.getItemId(), Ids);
}

bool usesLegacyModel(const ItemRecord &Record, const Catalog &Cat) {
  if (Record.Paint == 0) {
    return Record.Legacy;
  }
  const PaintKit *const Paint = Cat.paint(Record.Paint);
  return Paint != nullptr && Paint->LegacyModel;
}

void myController(CCSPlayerController &Controller) {
  CCSPlayerController_InventoryServices *const Services =
      Controller.m_pInventoryServices();
  if (Services == nullptr) {
    return;
  }
  const SchemaIds &Ids = Changer::get().catalog().Ids;
  const auto Items = Changer::get().loadout();
  if (const ItemRecord *const Music =
          Items->find(Ids.AnyTeam, Ids.Music, Ids)) {
    Services->m_unMusicID() = static_cast<std::uint16_t>(Music->Music);
  }
  if (const ItemRecord *const Medal =
          Items->find(Ids.AnyTeam, Ids.Medal, Ids)) {
    Services->m_rank()[MEDAL_CATEGORY_SEASON_COIN] = Medal->Definition;
  }
}

void dress(C_CSPlayerPawn &Pawn, ModelChanges &Model) {
  const Catalog &Cat = Changer::get().catalog();
  const auto Items = Changer::get().loadout();
  const std::uint8_t Team = Pawn.m_iTeamNum();
  const ItemRecord *const Gloves = Items->find(Team, Cat.Ids.Gloves, Cat.Ids);
  if (Gloves != nullptr) {
    show(Pawn.m_EconGloves(), *Gloves, Cat.Ids);
    Pawn.m_bNeedToReApplyGloves() = true;
  }
  const CatalogItem *const Agent = agentOf(*Items, Cat, Team);
  OC_LOG("skins", "dress pawn: team {} gloves {} agent {} model {}", Team,
         Gloves == nullptr ? 0 : Gloves->Definition,
         Agent == nullptr ? std::string("none") : Agent->Model, Model.sent());
  if (!Model.sent()) {
    return;
  }
  if (Agent != nullptr) {
    Model.setModel(Agent->Model.c_str());
  }
  if (Gloves != nullptr) {
    Model.hideDefaultGloves();
  }
}

void myPawn(C_CSPlayerPawn &Pawn, CNetworkFieldChangedEventQueue *Changes) {
  ModelChanges Model(Changes, Pawn);
  dress(Pawn, Model);
}

// Joining a team reuses the pawn without a full decode, so dress it again on spawn.
void onPlayerSpawn(IGameEvent &Event) {
  if (!Event.isLocalPlayer("userid")) {
    return;
  }
  if (CCSPlayerController *const My = CCSPlayerController::getLocal()) {
    myController(*My);
  }
  C_CSPlayerPawn *const Pawn = CCSPlayerController::getLocalPawn();
  CBodyComponentSkeletonInstance *const Body =
      Pawn == nullptr ? nullptr : Pawn->m_CBodyComponent();
  if (Body == nullptr) {
    return;
  }
  ModelChanges Model(*Pawn, Body->m_skeletonInstance().m_modelState());
  dress(*Pawn, Model);
}

GameEventHandler SpawnHandler{"player_spawn", onPlayerSpawn};

void myWeapon(C_EconEntity &Weapon, CNetworkFieldChangedEventQueue *Changes) {
  const Catalog &Cat = Changer::get().catalog();
  const auto Items = Changer::get().loadout();
  C_EconItemView &View = Weapon.getItem();
  const ItemRecord *const Record =
      recordOf(*Items, View, Weapon.m_iTeamNum(), Cat.Ids);
  if (Record == nullptr) {
    return;
  }
  show(View, *Record, Cat.Ids);
  const CatalogItem *const Knife =
      Record->Slot == Cat.Ids.Knife ? Cat.item(Record->Definition) : nullptr;
  if (Knife != nullptr) {
    Weapon.setSubclass(Knife->Name.c_str());
  }
  ModelChanges Model(Changes, Weapon);
  if (!Model.sent()) {
    return;
  }
  if (Knife != nullptr) {
    Model.setModel(Knife->Model.c_str());
  }
  if (usesLegacyModel(*Record, Cat)) {
    Model.useLegacyModel();
  }
}

void onDecoded(CEntityInstance &Entity,
               CNetworkFieldChangedEventQueue *Changes) {
  if (&Entity == CCSPlayerController::getLocal()) {
    myController(*Entity.as<CCSPlayerController>());
  } else if (auto *const Pawn = Entity.as<C_CSPlayerPawn>()) {
#ifdef OPENCHANGER_DEV
    static int Pawns = 0;
    if (Pawns < 40) {
      ++Pawns;
      OC_LOG("skins",
             "pawn {} class {}: controller handle {:#x}, local index {}, "
             "local pawn {:#x}, ours {}",
             Entity.getHandle(), Entity.getSchemaClassName(),
             Pawn->m_hController().m_Handle,
             CCSPlayerController::getLocalIndex(),
             CCSPlayerController::getLocal() == nullptr
                 ? 0U
                 : CCSPlayerController::getLocal()->m_hPlayerPawn().m_Handle,
             Pawn->hasLocalController());
    }
#endif
    if (Pawn->hasLocalController()) {
      myPawn(*Pawn, Changes);
    }
  } else if (auto *const Weapon = Entity.as<C_EconEntity>()) {
    const std::uint64_t Me = CCSInventoryManager::get()->getLocalSteamId();
    if (Me != 0 && Weapon->getOriginalOwner() == Me) {
      myWeapon(*Weapon, Changes);
    }
  }
}

} // namespace

} // namespace oc::changer
