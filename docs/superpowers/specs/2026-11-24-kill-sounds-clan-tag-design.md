# Kill sounds + clan tag (Extras tab)

## Why
Izzy wants a Gaia's Vengeance kill-streak sound and a custom clan tag in local bot matches. Offline-only like the rest of LarpChanger.

## Approaches
- **A (picked): local audio + local controller.** Play the user's own audio files via winmm on local kills; write `m_szClan` on the local `CCSPlayerController` on a timer.
- B: CS2 built-in sound events only — can't play custom files, rejected.
- C: hook the server-side clan setter in server.dll — more signatures to break per update, no visible gain in a listen-server bot match.

## Design
- **Kill sound packs** in `%APPDATA%\openchanger\killsounds\<pack>\`. One file = same sound every kill; numbered `1..N` = successive, kill N plays file `((N-1) mod count)+1`, so a streak past the last sound (deathmatch) starts over at 1.
- Streak counter resets on `round_start` and on local player death.
- Trigger: existing `player_death` handling, when the local player is the attacker (and not suicide).
- Settings (in `extras.json`): pack name (or off), volume 0–100.
- Extras tab: pack dropdown, volume slider, preview button, open-folder button.
- Two original synthesized packs ship in the installer; Izzy's Gaia pack is split locally into 5 files and not distributed.
- **Clan tag:** text (max 12 chars), mode Static / Scroll / Type / Blink, speed slider; applied to the local controller's `m_szClan` every tick of its timer (~150 ms base), cleared when disabled.

## Non-goals
Online-visible tags, bundled third-party audio, sound pack downloads.

## Left to the builder
Exact UI layout (reuse existing components), synthesized sound design, timer mechanism (game thread vs. existing frame hook).
