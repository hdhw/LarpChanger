#include "features/extras/Extras.h"

#include "core/log.h"
#include "core/settings.h"
#include "features/extras/decode.h"
#include "sdk/client/CCSPlayerController.h"
#include "sdk/tier0/IMemAlloc.h"

#include <windows.h>

#include <mfapi.h>
#include <mmsystem.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace oc::extras {

namespace {

constexpr std::size_t MaxTag = 12;
constexpr std::uint64_t BaseStepMs = 150;

bool isAudio(const std::filesystem::path &File) {
  std::wstring Extension = File.extension().wstring();
  std::ranges::transform(Extension, Extension.begin(), ::towlower);
  return Extension == L".mp3" || Extension == L".wav";
}

std::filesystem::path packPath(const std::string &Name) {
  return {std::u8string(Name.begin(), Name.end())};
}

void stopSound() { PlaySoundW(nullptr, nullptr, 0); }

// The scoreboard reads the sanitized copy, a game-heap CUtlString, so it must be allocated and freed by tier0.
void showTag(CCSPlayerController &My, const char *Text) {
  My.m_szClan() = *Text != 0 ? Text : nullptr;
  CUtlString &Shown = My.m_sSanitizedClanTag();
  if (std::string_view(Shown.get()) == Text) {
    return;
  }
  const char *const Old = Shown.m_pString;
  Shown.m_pString = IMemAlloc::StrDup(Text);
  if (Old != nullptr) {
    IMemAlloc::FreeBlock(const_cast<char *>(Old));
  }
}

} // namespace

Extras &Extras::get() {
  static Extras Instance;
  return Instance;
}

std::filesystem::path Extras::soundFolder() {
  return userDirectory() / L"killsounds";
}

bool Extras::start() {
  std::error_code Error;
  std::filesystem::create_directories(soundFolder(), Error);
  Now = loadSettings<Settings>(*this);
  Audio = std::jthread([this](const std::stop_token &Stop) { audioLoop(Stop); });
  loadPack();
  OC_LOG("extras", "start: pack '{}' ({} files), tag '{}'", Now.Pack,
         Files.size(), Now.Tag);
  return true;
}

void Extras::stop() {
  post([this] {
    stopSound();
    Sounds.clear();
  });
  Audio.request_stop();
  Wake.notify_all();
  if (Audio.joinable()) {
    Audio.join();
  }
  if (TagShown) {
    if (CCSPlayerController *const My = CCSPlayerController::getLocal()) {
      showTag(*My, "");
    }
    TagShown = false;
  }
}

std::vector<std::string> Extras::packs() {
  std::vector<std::string> Names;
  std::error_code Error;
  for (const auto &Entry :
       std::filesystem::directory_iterator(soundFolder(), Error)) {
    if (Entry.is_directory()) {
      const std::u8string Name = Entry.path().filename().u8string();
      Names.emplace_back(Name.begin(), Name.end());
    }
  }
  std::ranges::sort(Names);
  return Names;
}

void Extras::loadPack() {
  Files.clear();
  Kills = 0;
  if (Now.Pack.empty()) {
    return;
  }
  std::error_code Error;
  std::vector<std::pair<int, std::filesystem::path>> Found;
  for (const auto &Entry : std::filesystem::directory_iterator(
           soundFolder() / packPath(Now.Pack), Error)) {
    if (!Entry.is_regular_file() || !isAudio(Entry.path())) {
      continue;
    }
    const std::string Stem = Entry.path().stem().string();
    const bool Numbered =
        !Stem.empty() && std::ranges::all_of(Stem, [](unsigned char C) {
          return std::isdigit(C) != 0;
        });
    Found.emplace_back(Numbered ? std::stoi(Stem) : 0, Entry.path());
  }
  std::ranges::sort(Found);
  for (auto &[Order, File] : Found) {
    Files.push_back(std::move(File));
  }
  post([this, Paths = Files, Volume = Now.Volume] {
    stopSound();
    Sounds.clear();
    for (const std::filesystem::path &Path : Paths) {
      Sounds.push_back(decodeToWav(Path, Volume));
    }
    OC_LOG("extras", "decoded {} sounds", Sounds.size());
  });
}

void Extras::play(std::size_t Index) {
  if (Index >= Files.size()) {
    return;
  }
  post([this, Index] {
    if (Index < Sounds.size() && !Sounds[Index].empty()) {
      PlaySoundW(reinterpret_cast<LPCWSTR>(Sounds[Index].data()), nullptr,
                 SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }
  });
}

void Extras::post(std::function<void()> Job) {
  {
    const std::scoped_lock Lock(Mutex);
    Jobs.push_back(std::move(Job));
  }
  Wake.notify_one();
}

// Decoding takes ~1s per pack, so it and the buffers PlaySound reads live on this thread.
void Extras::audioLoop(const std::stop_token &Stop) {
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  MFStartup(MF_VERSION, MFSTARTUP_LITE);
  for (;;) {
    std::function<void()> Job;
    {
      std::unique_lock Lock(Mutex);
      Wake.wait(Lock, Stop, [this] { return !Jobs.empty(); });
      if (Jobs.empty()) {
        break;
      }
      Job = std::move(Jobs.front());
      Jobs.pop_front();
    }
    Job();
  }
  MFShutdown();
  CoUninitialize();
}

void Extras::setSound(std::string Pack, int Volume) {
  Now.Pack = std::move(Pack);
  Now.Volume = std::clamp(Volume, 0, 100);
  loadPack();
  saveSettings(*this, Now);
}

void Extras::previewSound() { play(0); }

void Extras::onKill() {
  ++Kills;
  play(streakSound(Kills, Files.size()));
}

void Extras::setTag(std::string Text, TagMode Mode, int Speed, bool On) {
  if (Text.size() > MaxTag) {
    Text.resize(MaxTag);
  }
  Now.Tag = std::move(Text);
  Now.Mode = Mode;
  Now.Speed = std::clamp(Speed, 1, 100);
  Now.TagOn = On;
  saveSettings(*this, Now);
}

void Extras::tick() {
  CCSPlayerController *const My = CCSPlayerController::getLocal();
  if (My == nullptr) {
    return;
  }
  if (!Now.TagOn) {
    if (TagShown) {
      showTag(*My, "");
      TagShown = false;
    }
    return;
  }
  const std::uint64_t StepMs =
      std::max<std::uint64_t>(BaseStepMs * 50 / Now.Speed, 1);
  Frame = tagFrame(Now.Tag, Now.Mode, GetTickCount64() / StepMs);
  showTag(*My, Frame.c_str());
  TagShown = true;
}

void Extras::openFolder() {
  std::error_code Error;
  std::filesystem::create_directories(soundFolder(), Error);
  ShellExecuteW(nullptr, L"open", soundFolder().c_str(), nullptr, nullptr,
                SW_SHOWNORMAL);
}

} // namespace oc::extras
