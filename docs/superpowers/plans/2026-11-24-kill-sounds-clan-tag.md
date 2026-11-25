# Kill Sounds + Clan Tag Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An Extras tab in the LarpChanger menu with successive kill-sound packs and a static/animated clan tag.

**Architecture:** A new `extras` Feature (own `extras.json`) next to `changer`. Pure logic (streak index, tag frames) lives in a header of `constexpr` functions checked by `static_assert`s, since the repo has no test runner. Sounds play through winmm MCI from the `player_death` event; the clan tag is written to the local controller's `m_szClan` from a native `tick()` that the Panorama UI calls on `$.Schedule`, with the frame derived from wall-clock time so tick jitter doesn't matter.

**Tech Stack:** C++23 (clang-cl), glaze settings, winmm, SolidJS Panorama UI (`src/ui/panorama`).

**Spec:** `docs/superpowers/specs/2026-11-24-kill-sounds-clan-tag-design.md`

## Global Constraints

- Offline only: nothing new runs unless the DLL already passed the `-insecure` gate.
- Packs live in `%APPDATA%\openchanger\killsounds\<pack>\` (`userDirectory() / "killsounds"`).
- Successive pack = files named `1..N` (any of `.mp3`/`.wav`); kill N plays `min(N, count)`. Any other single file = same sound every kill.
- Streak resets on `round_start` and on local player death.
- Clan tag max 12 characters; modes `static | scroll | type | blink`; base step 150 ms scaled by speed.
- No third-party audio in the repo or release. Gaia pack is created locally only.
- No code comments except a one-line why where needed (Izzy's rule).

## Review Focus

- Pack folder deleted or emptied while selected → no sound, no crash; dropdown shows "Off".
- Filenames with spaces/apostrophes (e.g. `Gaia's Vengeance`) → MCI open still works (quote the path).
- Two kills within the same sound → new sound cuts the old one (close alias first), never stacks forever.
- Empty clan tag with animated mode → `static_assert` that every mode returns `""` for `""`.
- Disabling the tag → `m_szClan` reset to `""` once, not left on the last frame.

---

### Task 1: Pure logic header

**Files:**
- Create: `src/features/extras/logic.h`

**Interfaces:**
- Produces: `enum class TagMode : std::uint8_t { Static, Scroll, Type, Blink };`
- Produces: `constexpr std::size_t streakSound(std::size_t Kill, std::size_t Count)` → 0-based index, `Kill` is 1-based; returns `0` when `Count <= 1`.
- Produces: `constexpr std::string tagFrame(std::string_view Text, TagMode Mode, std::uint64_t Step)`

- [ ] **Step 1: Write `static_assert`s at the bottom of `logic.h`**
  - `streakSound(1,5)==0`, `streakSound(5,5)==4`, `streakSound(9,5)==4`, `streakSound(3,1)==0`
  - `tagFrame("LARP",Static,7)=="LARP"`
  - Scroll rotates by one char per step: `tagFrame("LARP",Scroll,1)=="ARPL"`, step 4 back to `"LARP"`
  - Type grows then holds: step 0 `"L"`, step 3 `"LARP"`, steps Size..2*Size-1 hold full text, then restarts
  - Blink alternates full/empty: step 0 `"LARP"`, step 1 `""`
  - Every mode with `""` returns `""`
- [ ] **Step 2: Implement both functions so the asserts compile** (constexpr std::string, C++20).
- [ ] **Step 3: Verify** — `tools\build.cmd dev` builds once `logic.h` is included from Task 2's .cpp.

### Task 2: Extras feature (settings, sounds, clan tag)

**Files:**
- Create: `src/features/extras/Extras.h`, `src/features/extras/Extras.cpp`, `src/features/extras/kills.cpp`, `src/features/extras/CMakeLists.txt`
- Modify: `src/app/features.cpp` (add `&extras::Extras::get()`), root `CMakeLists.txt` (add subdirectory, link `winmm` to `oc_objects`)
- Modify: `src/sdk/client/CCSPlayerController.h` (add `SCHEMA_FIELD(const char *, m_szClan);`)

**Interfaces:**
- Consumes: `logic.h` from Task 1; `loadSettings/saveSettings/userDirectory` (`core/settings.h`); `GameEventHandler`, `IGameEvent::isLocalPlayer`, `CCSPlayerController::getLocal()`.
- Produces (`oc::extras::Extras`, singleton via `get()`, `name()` = `"extras"`):
  - `struct Settings { std::string Pack; int Volume = 80; std::string Tag; TagMode Mode = TagMode::Static; int Speed = 50; bool TagOn = false; };`
  - `std::vector<std::string> packs() const` — sorted subfolder names of `killsounds`
  - `void setSound(std::string Pack, int Volume)`; `void previewSound()` (plays file 1)
  - `void setTag(std::string Text, TagMode Mode, int Speed, bool On)`
  - `void tick()` — writes the current frame into `m_szClan` when `TagOn`; writes `""` once on turning off
  - `void openFolder()` — `ShellExecuteW` on the killsounds folder (create it first)
  - `void onKill()`, `void resetStreak()`

- [ ] **Step 1: `Extras.h/.cpp`** — `start()` loads settings and creates `killsounds\`; setters clamp (Volume 0–100, Speed 1–100, Tag truncated to 12 chars) then `saveSettings`.
- [ ] **Step 2: Sound playback** — resolve pack files once per `setSound`; play with MCI: `close larpkill`, `open "<path>" type mpegvideo alias larpkill`, `setaudio larpkill volume to <Volume*10>`, `play larpkill` (async, no `wait`).
- [ ] **Step 3: Clan tag** — step = `GetTickCount64() / (150 * 50 / Speed)`; keep the frame in a member `std::string` so the pointer given to `m_szClan` stays valid.
- [ ] **Step 4: `kills.cpp`** — `GameEventHandler` for `player_death` (local attacker and not local victim → `onKill()`; local victim → `resetStreak()`) and `round_start` → `resetStreak()`.
- [ ] **Step 5: Verify** — `tools\build.cmd dev` succeeds; inject; log shows `extras` started and no missing links (`m_szClan` resolves).
- [ ] **Step 6: Commit** `feat: extras feature with kill sound packs and clan tag`

### Task 3: JS bridge + Extras tab

**Files:**
- Create: `src/ui/extras/methods.cpp` (JsMethods under namespace `"extras"`: `packs`, `settings`, `setSound(pack, volume)`, `previewSound`, `setTag(text, mode, speed, on)`, `tick`, `openFolder`), `src/ui/panorama/src/features/changer/screens/Extras.tsx` + `Extras.module.css`
- Modify: `browser.ts` (`Mode` adds `'extras'`), `Header.tsx` (Extras tab after Mine), `Changer.tsx` (`<Match when={mode() === 'extras'}>`), i18n strings
- Modify: `src/ui/panorama/src/main.tsx` or app boot — start a `$.Schedule(0.1, …)` loop that calls `OpenChangerAPI.extras.tick()` forever, independent of the tab being open

**Interfaces:**
- Consumes: Task 2's `Extras` methods; existing components `DropButton`, `SliderField`, `TextField`, `Segmented`, `ActionButton`, `FieldLabel`.
- Produces: `OpenChangerAPI.extras.*` (types regenerate into `generated/api` via `oc_api_types`).

- [ ] **Step 1: Bridge methods**, `JsEnum<extras::TagMode>` names `static, scroll, type, blink`.
- [ ] **Step 2: Extras screen** — Kill sound: pack dropdown (with "Off"), volume slider, Preview, Open folder. Clan tag: on/off, text field (maxlength 12), mode segmented, speed slider (hidden for Static). Every change calls the setter immediately.
- [ ] **Step 3: Verify** — `tools\build.cmd release` (bundles frontend); dev inject; tab appears, controls persist across re-inject (`extras.json`).
- [ ] **Step 4: Commit** `feat: Extras tab for kill sounds and clan tag`

### Task 4: Sound packs + in-game check + release

**Files:**
- Create: `assets/killsounds/` with two original synthesized packs (ffmpeg `sine`/`aevalsrc`): `Ping` (single file) and `Rising` (1–5 ascending tones)
- Modify: `tools/installer.cpp` + `installer.rc` — embed and unpack them to `%APPDATA%\openchanger\killsounds\` without overwriting existing files
- Local only: split `C:\Users\izzy\Downloads\Gaia_s Vengeance Kill Successive.mp3` at 1.73 / 3.58 / 5.36 / 7.05 s into `%APPDATA%\openchanger\killsounds\Gaia's Vengeance\1..5.mp3`

- [ ] **Step 1: Generate packs, wire installer.**
- [ ] **Step 2: Split Gaia locally**; listen-check durations with ffprobe (~1.7, 1.9, 1.8, 1.7, 4.8 s).
- [ ] **Step 3: In-game** — bot match: kills 1→5 escalate, kill 6 repeats 5, dying/new round restarts at 1; tag shows on scoreboard in each mode; dev log clean.
- [ ] **Step 4: Release** — bump `LARP_VERSION` to `1.2.0`, commit, tag `v1.2.0`, push, `gh release create` with notes (offline warning, new Extras tab, how to add your own pack).
