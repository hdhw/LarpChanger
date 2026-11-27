#pragma once

#include "core/feature.h"
#include "features/extras/logic.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace oc::extras {

class Extras final : public Feature {
public:
  static Extras &get();

  [[nodiscard]] std::string_view name() const override { return "extras"; }
  bool start() override;
  void stop() override;

  [[nodiscard]] const Settings &settings() const { return Now; }
  [[nodiscard]] static std::vector<std::string> packs();
  static std::filesystem::path soundFolder();
  static std::filesystem::path mvpFolder();
  [[nodiscard]] static std::vector<std::string> songs();

  void setSound(std::string Pack, int Volume);
  void previewSound();
  void setTag(std::string Text, TagMode Mode, int Speed, bool On);
  void tick();
  static void openFolder(const std::filesystem::path &Folder);

  void setMvp(std::string Song, int Volume, std::string Title,
              std::string Artist);
  [[nodiscard]] std::string mvpIcon() const;
  void playMvp();
  void stopMvp();
  void onMvp();
  [[nodiscard]] bool mutes(std::uint32_t Sound) const;

  void onKill();
  void resetStreak() { Kills = 0; }

private:
  void loadPack();
  void loadMvp();
  void play(std::size_t Index);
  void post(std::function<void()> Job);
  void audioLoop(const std::stop_token &Stop);

  Settings Now;
  std::vector<std::filesystem::path> Files;
  std::size_t Kills = 0;
  std::string Frame;
  bool TagShown = false;

  std::mutex Mutex;
  std::condition_variable_any Wake;
  std::deque<std::function<void()>> Jobs;
  std::vector<std::vector<std::byte>> Sounds;
  std::vector<std::byte> Song;
  std::vector<std::uint32_t> Anthems;
  bool Decoding = false;
  std::jthread Audio;
};

} // namespace oc::extras
