#include "app/features.h"

#include "core/feature.h"
#include "features/changer/Changer.h"
#include "features/extras/Extras.h"

#include <array>
#include <span>

namespace oc {

std::span<Feature *const> features() {
  static const std::array<Feature *, 2> List{
      &changer::Changer::get(),
      &extras::Extras::get(),
  };
  return List;
}

} // namespace oc
