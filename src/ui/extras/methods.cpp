#include "features/extras/Extras.h"
#include "game/js.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace oc {

template <> struct JsEnum<extras::TagMode> {
  static constexpr std::string_view Name = "TagMode";
  static constexpr std::array<std::string_view, 4> Names{
      "static",
      "scroll",
      "type",
      "blink",
  };
};

namespace {

using extras::Extras;

std::vector<std::string> packs() { return Extras::packs(); }

extras::Settings settings() { return Extras::get().settings(); }

void setSound(std::string Pack, int Volume) {
  Extras::get().setSound(std::move(Pack), Volume);
}

void previewSound() { Extras::get().previewSound(); }

void setTag(std::string Text, extras::TagMode Mode, int Speed, bool On) {
  Extras::get().setTag(std::move(Text), Mode, Speed, On);
}

void tick() { Extras::get().tick(); }

void openFolder() { Extras::openFolder(); }

JsMethod<&packs> Packs{"extras", "packs"};
JsMethod<&settings> Settings{"extras", "settings"};
JsMethod<&setSound> SetSound{"extras", "setSound", "pack", "volume"};
JsMethod<&previewSound> PreviewSound{"extras", "previewSound"};
JsMethod<&setTag> SetTag{"extras", "setTag", "text", "mode", "speed", "on"};
JsMethod<&tick> Tick{"extras", "tick"};
JsMethod<&openFolder> OpenFolder{"extras", "openFolder"};

} // namespace

} // namespace oc
