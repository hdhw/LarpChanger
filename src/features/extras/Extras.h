#pragma once

#include "core/feature.h"
#include "features/extras/logic.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace oc::extras {

struct Settings {
  std::string Pack;
  int Volume = 80;
  std::string Tag;
  TagMode Mode = TagMode::Static;
  int Speed = 50;
  bool TagOn = false;
};

class Extras final : public Feature {
public:
  static Extras &get();

  [[nodiscard]] std::string_view name() const override { return "extras"; }
  bool start() override;
  void stop() override;

  [[nodiscard]] const Settings &settings() const { return Now; }
  [[nodiscard]] static std::vector<std::string> packs();
  static std::filesystem::path soundFolder();

  void setSound(std::string Pack, int Volume);
  void previewSound();
  void setTag(std::string Text, TagMode Mode, int Speed, bool On);
  void tick();
  static void openFolder();

  void onKill();
  void resetStreak() { Kills = 0; }

private:
  void loadPack();
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
  std::jthread Audio;
};

} // namespace oc::extras
