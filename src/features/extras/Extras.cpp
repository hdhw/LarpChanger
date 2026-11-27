#include "features/extras/Extras.h"

#include "core/log.h"
#include "core/settings.h"
#include "features/changer/Changer.h"
#include "features/extras/decode.h"
#include "game/panorama.h"
#include "sdk/client/CCSPlayerController.h"
#include "sdk/soundsystem/CSoundOpSystem.h"
#include "sdk/tier0/IMemAlloc.h"

#include <windows.h>

#include <mfapi.h>
#include <mmsystem.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace oc::extras {

namespace {

constexpr std::uint64_t BaseStepMs = 150;

std::wstring extensionOf(const std::filesystem::path &File) {
  std::wstring Extension = File.extension().wstring();
  std::ranges::transform(Extension, Extension.begin(), ::towlower);
  return Extension;
}

bool isAudio(const std::filesystem::path &File) {
  const std::wstring Extension = extensionOf(File);
  return Extension == L".mp3" || Extension == L".wav";
}

// Exceptions are off in this DLL, so only the error_code filesystem overloads are safe here.
template <class Visit>
void forEachEntry(const std::filesystem::path &Folder, Visit &&Each) {
  std::error_code Error;
  std::filesystem::directory_iterator It(Folder, Error);
  for (const std::filesystem::directory_iterator End; !Error && It != End;
       It.increment(Error)) {
    Each(*It);
  }
}

std::filesystem::path packPath(const std::string &Name) {
  return {std::u8string(Name.begin(), Name.end())};
}

std::string utf8Name(const std::filesystem::path &Path) {
  const std::u8string Name = Path.filename().u8string();
  return {Name.begin(), Name.end()};
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

std::filesystem::path Extras::mvpFolder() { return userDirectory() / L"mvp"; }

bool Extras::start() {
  std::error_code Error;
  std::filesystem::create_directories(soundFolder(), Error);
  std::filesystem::create_directories(mvpFolder(), Error);
  Now = sanitized(loadSettings<Settings>(*this));
  Audio = std::jthread([this](const std::stop_token &Stop) { audioLoop(Stop); });
  loadPack();
  loadMvp();
  OC_LOG("extras", "start: pack '{}' ({} files), mvp '{}', tag '{}'",
         Now.Pack, Files.size(), Now.Mvp, Now.Tag);
  return true;
}

void Extras::stop() {
  post([this] {
    stopSound();
    Sounds.clear();
    Song.clear();
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
  forEachEntry(soundFolder(), [&](const auto &Entry) {
    std::error_code Error;
    if (Entry.is_directory(Error)) {
      Names.push_back(utf8Name(Entry.path()));
    }
  });
  std::ranges::sort(Names);
  return Names;
}

void Extras::loadPack() {
  Files.clear();
  Kills = 0;
  if (Now.Pack.empty()) {
    return;
  }
  std::vector<std::pair<int, std::filesystem::path>> Found;
  forEachEntry(soundFolder() / packPath(Now.Pack), [&](const auto &Entry) {
    std::error_code Error;
    if (Entry.is_regular_file(Error) && isAudio(Entry.path())) {
      Found.emplace_back(packOrder(Entry.path().stem().native()),
                         Entry.path());
    }
  });
  std::ranges::sort(Found);
  for (auto &[Order, File] : Found) {
    Files.push_back(std::move(File));
  }
  post([this, Paths = Files, Volume = Now.Volume] {
    stopSound();
    Sounds.clear();
    for (const std::filesystem::path &Path : Paths) {
      Sounds.push_back(Decoding ? decodeToWav(Path, Volume)
                                : std::vector<std::byte>());
    }
    OC_LOG("extras", "decoded {} sounds", Sounds.size());
  });
}

std::vector<std::string> Extras::songs() {
  std::vector<std::string> Names;
  forEachEntry(mvpFolder(), [&](const auto &Entry) {
    std::error_code Error;
    if (Entry.is_regular_file(Error) && isAudio(Entry.path())) {
      Names.push_back(utf8Name(Entry.path()));
    }
  });
  std::ranges::sort(Names);
  return Names;
}

void Extras::loadMvp() {
  std::filesystem::path Path;
  if (!Now.Mvp.empty()) {
    Path = mvpFolder() / packPath(Now.Mvp);
  }
  post([this, Path = std::move(Path), Volume = Now.MvpVolume] {
    stopSound();
    Song = Decoding && !Path.empty() ? decodeToWav(Path, Volume)
                                     : std::vector<std::byte>();
    OC_LOG("extras", "mvp song decoded: {} bytes", Song.size());
  });
}

void Extras::setMvp(std::string Name, int Volume, std::string Title,
                    std::string Artist) {
  const bool Reload = Name != Now.Mvp || Volume != Now.MvpVolume;
  Now.Mvp = std::move(Name);
  Now.MvpVolume = Volume;
  Now.MvpTitle = std::move(Title);
  Now.MvpArtist = std::move(Artist);
  Now = sanitized(std::move(Now));
  if (Reload) {
    loadMvp();
  }
  saveSettings(*this, Now);
}

// The cover is any png/jpg in the mvp folder named like the song, e.g. paparazzi.png.
std::string Extras::mvpIcon() const {
  if (Now.Mvp.empty()) {
    return {};
  }
  const std::filesystem::path Stem = packPath(Now.Mvp).stem();
  std::string Url;
  forEachEntry(mvpFolder(), [&](const auto &Entry) {
    const std::wstring Extension = extensionOf(Entry.path());
    if (Url.empty() && Entry.path().stem() == Stem &&
        (Extension == L".png" || Extension == L".jpg" ||
         Extension == L".jpeg")) {
      const std::u8string Path = Entry.path().generic_u8string();
      Url = "file://" + std::string(Path.begin(), Path.end());
    }
  });
  return Url;
}

void Extras::playMvp() {
  post([this] {
    if (!Song.empty()) {
      PlaySoundW(reinterpret_cast<LPCWSTR>(Song.data()), nullptr,
                 SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }
  });
}

void Extras::stopMvp() {
  if (!Now.Mvp.empty()) {
    post(stopSound);
  }
}

void Extras::onMvp() {
  if (Now.Mvp.empty()) {
    return;
  }
  const auto &Kits = changer::Changer::get().catalog().MusicKits;
  if (Anthems.size() != Kits.size() + 1) {
    Anthems.assign(1, CSoundOpSystem::getSoundEventHash("Music.MVPAnthem"));
    for (const changer::Kit &Kit : Kits) {
      const std::string Name = "Music.MVPAnthem." + Kit.Name;
      Anthems.push_back(CSoundOpSystem::getSoundEventHash(Name.c_str()));
    }
  }
  playMvp();
  notify("extras", "mvp");
}

bool Extras::mutes(std::uint32_t Sound) const {
  return !Now.Mvp.empty() && std::ranges::contains(Anthems, Sound);
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
  // Media Foundation is delay-loaded: Windows N without the Media Feature Pack has no mfplat.dll.
  Decoding = LoadLibraryW(L"mfplat.dll") != nullptr &&
             LoadLibraryW(L"mfreadwrite.dll") != nullptr &&
             SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE));
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
  if (Decoding) {
    MFShutdown();
  }
  CoUninitialize();
}

void Extras::setSound(std::string Pack, int Volume) {
  Now.Pack = std::move(Pack);
  Now.Volume = Volume;
  Now = sanitized(std::move(Now));
  loadPack();
  saveSettings(*this, Now);
}

void Extras::previewSound() { play(0); }

void Extras::onKill() {
  ++Kills;
  play(streakSound(Kills, Files.size()));
}

void Extras::setTag(std::string Text, TagMode Mode, int Speed, bool On) {
  Now.Tag = std::move(Text);
  Now.Mode = Mode;
  Now.Speed = Speed;
  Now.TagOn = On;
  Now = sanitized(std::move(Now));
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

void Extras::openFolder(const std::filesystem::path &Folder) {
  std::error_code Error;
  std::filesystem::create_directories(Folder, Error);
  ShellExecuteW(nullptr, L"open", Folder.c_str(), nullptr, nullptr,
                SW_SHOWNORMAL);
}

} // namespace oc::extras
