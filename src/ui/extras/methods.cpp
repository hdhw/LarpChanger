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

void openFolder() { Extras::openFolder(Extras::soundFolder()); }

std::vector<std::string> songs() { return Extras::songs(); }

void setMvp(std::string Song, int Volume, std::string Title,
            std::string Artist) {
  Extras::get().setMvp(std::move(Song), Volume, std::move(Title),
                       std::move(Artist));
}

std::string mvpIcon() { return Extras::get().mvpIcon(); }

void previewMvp() { Extras::get().playMvp(); }

void openMvpFolder() { Extras::openFolder(Extras::mvpFolder()); }

JsMethod<&packs> Packs{"extras", "packs"};
JsMethod<&settings> Settings{"extras", "settings"};
JsMethod<&setSound> SetSound{"extras", "setSound", "pack", "volume"};
JsMethod<&previewSound> PreviewSound{"extras", "previewSound"};
JsMethod<&setTag> SetTag{"extras", "setTag", "text", "mode", "speed", "on"};
JsMethod<&tick> Tick{"extras", "tick"};
JsMethod<&openFolder> OpenFolder{"extras", "openFolder"};
JsMethod<&songs> Songs{"extras", "songs"};
JsMethod<&setMvp> SetMvp{"extras", "setMvp", "song", "volume", "title",
                         "artist"};
JsMethod<&mvpIcon> MvpIcon{"extras", "mvpIcon"};
JsMethod<&previewMvp> PreviewMvp{"extras", "previewMvp"};
JsMethod<&openMvpFolder> OpenMvpFolder{"extras", "openMvpFolder"};

} // namespace

} // namespace oc
