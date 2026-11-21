#include <windows.h>

#include <shellapi.h>

#include "app/app.h"
#include "app/features.h"
#include "core/feature.h"
#include "core/log.h"
#include "game/hook.h"
#include "game/link.h"
#include "sdk/sdl3/SDL.h"
#include "ui/ui.h"

#include <cstddef>
#include <ranges>
#include <span>

namespace oc::app {

namespace {

bool Running = false;

void stopFeatures(std::span<Feature *const> Started) {
  for (Feature *const Each : std::views::reverse(Started)) {
    Each->stop();
  }
}

void start() {
  const std::size_t Missing = resolveLinks();
#ifdef OPENCHANGER_DEV
  logStart(Missing);
#endif
  if (Missing != 0) {
    OC_LOG("app", "{} links missing, not started", Missing);
    return;
  }

  const std::span<Feature *const> Features = features();

  for (std::size_t Started = 0; Started < Features.size(); ++Started) {
    if (!Features[Started]->start()) {
      OC_LOG("app", "{} did not start", Features[Started]->name());
      stopFeatures(Features.first(Started));
      return;
    }
  }

  Running = true;

  if (!installHooks() || !ui::start()) {
    OC_LOG("app", "hooks or the UI did not start");
    unhook();
    SDL::RunOnMainThread([](void *) { stop(); }, nullptr, false);
  }
}

} // namespace

bool running() { return Running; }

void unhook() {
  ui::stop();
  removeHooks();
}

void stop() {
  if (!Running) {
    return;
  }

  Running = false;
  OC_LOG("app", "stop");
  stopFeatures(features());
}

} // namespace oc::app

namespace {

bool launchedInsecure() {
  int Count = 0;
  LPWSTR *const Args = CommandLineToArgvW(GetCommandLineW(), &Count);
  if (Args == nullptr) {
    return false;
  }
  bool Found = false;
  for (int Each = 1; Each < Count && !Found; ++Each) {
    Found = _wcsicmp(Args[Each], L"-insecure") == 0;
  }
  LocalFree(Args);
  return Found;
}

} // namespace

extern "C" BOOL WINAPI DllMain([[maybe_unused]] HINSTANCE Instance,
                               DWORD Reason, [[maybe_unused]] LPVOID Reserved) {
  // Offline use only: without -insecure VAC is active, so refuse to load at all.
  if (Reason == DLL_PROCESS_ATTACH && !launchedInsecure()) {
    return FALSE;
  }
  if (Reason == DLL_PROCESS_ATTACH &&
      oc::resolveLink(SDL::RunOnMainThreadLink)) {
    SDL::RunOnMainThread([](void *) { oc::app::start(); }, nullptr, false);
  }
  return TRUE;
}