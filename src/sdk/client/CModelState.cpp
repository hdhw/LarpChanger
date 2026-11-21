#include "sdk/client/CModelState.h"

#include "core/log.h"

#include "sdk/client/CEntityInstance.h"
#include "sdk/client/CModel.h"
#include "sdk/networksystem/CNetworkFieldChangedEventQueue.h"
#include "sdk/resourcesystem/CResourceName.h"
#include "sdk/resourcesystem/CResourceSystem.h"
#include "sdk/resourcesystem/ResourceBinding_t.h"

#include <cstddef>
#include <cstdint>

ModelChanges::ModelChanges(CNetworkFieldChangedEventQueue *Queue,
                           CEntityInstance &Entity)
    : Queue(Queue), Entity(Entity) {
  if (Queue == nullptr) {
    return;
  }
  const auto Model =
      Queue->findChange(Entity, CModelState::SkeletonModelChangedLink,
                        CNetworkFieldChangedEventQueue::NoElement);
  const auto Mask =
      Queue->findChange(Entity, CModelState::SkeletonMeshGroupMaskChangedLink,
                        CNetworkFieldChangedEventQueue::NoElement);
  State = static_cast<CModelState *>(Model.Object != nullptr ? Model.Object
                                                             : Mask.Object);
  SentModel = static_cast<ResourceBinding_t **>(Model.Value);
  SentMask = static_cast<std::uint64_t *>(Mask.Value);
}

void ModelChanges::setModel(const char *Name) {
  if (State == nullptr) {
    return;
  }
  const CResourceName Resource(Name);
  ResourceBinding_t *const Binding =
      CResourceSystem::get()->FindResidentBinding(Resource.m_nResourceId,
                                                  CModel::ResourceType);
  if (Binding == nullptr) {
    OC_LOG("model", "{} is not loaded, model not swapped", Name);
    return;
  }
  OC_LOG("model", "swapped to {} ({})", Name,
         SentModel != nullptr ? "in update" : "directly");
  if (SentModel != nullptr) {
    *SentModel = Binding;
  } else if (Queue == nullptr) {
    // Hand the swap to the game's own network callback so it rebuilds skeleton, animation and body groups.
    // shortcut: no binding addref, like the in-update path; fine while the model stays resident.
    Pending = Binding;
    CModelState::SkeletonModelChanged(State, &Entity, &Pending);
  } else if (State->m_hModel().m_pBinding != Binding) {
    CModelState::SetModel(State, Binding);
  }
}

void ModelChanges::hideDefaultGloves() {
  CModel *const Agent = model();
  if (Agent == nullptr) {
    return;
  }
  const std::int32_t Group =
      CModel::FindBodyGroupByName(Agent, CModel::GlovesBodyGroup);
  std::int32_t *const Choice = choice(Group);
  if (Choice == nullptr) {
    OC_LOG("model", "no gloves body group ({}), default gloves stay", Group);
    return;
  }
  *Choice = Agent->getHiddenGlovesChoice(Group, *Choice);
  OC_LOG("model", "default gloves hidden");
  CModel::UpdateMeshGroupMask(Agent, &mask(), Group, *Choice);
}

void ModelChanges::useLegacyModel() {
  CModel *const Weapon = model();
  if (Weapon == nullptr) {
    return;
  }
  for (const char *const Name : CModel::LegacyBodyGroups) {
    const std::int32_t Group = CModel::FindBodyGroupByName(Weapon, Name);
    const std::int32_t Legacy = Weapon->getLegacyChoice(Group);
    std::int32_t *const Choice = choice(Group);
    if (Legacy >= 0 && Choice != nullptr) {
      *Choice = Legacy;
      CModel::UpdateMeshGroupMask(Weapon, &mask(), Group, Legacy);
    }
  }
}

CModel *ModelChanges::model() {
  if (State == nullptr) {
    return nullptr;
  }
  const ResourceBinding_t *const Binding =
      Pending != nullptr     ? Pending
      : SentModel == nullptr ? State->m_hModel().m_pBinding
                             : *SentModel;
  return Binding == nullptr ? nullptr : static_cast<CModel *>(Binding->m_pData);
}

std::uint64_t &ModelChanges::mask() {
  return SentMask == nullptr ? State->m_MeshGroupMask() : *SentMask;
}

std::int32_t *ModelChanges::choice(std::int32_t Group) {
  if (Group < 0 || Group >= State->m_nBodyGroupChoices().m_Size) {
    return nullptr;
  }
  void *const Sent =
      Queue == nullptr
          ? nullptr
          : Queue
                ->findChange(Entity, CModelState::BodyGroupChoiceChangedLink,
                             Group)
                .Value;
  return Sent == nullptr ? &State->m_nBodyGroupChoices()
                                .span()[static_cast<std::size_t>(Group)]
                         : static_cast<std::int32_t *>(Sent);
}
