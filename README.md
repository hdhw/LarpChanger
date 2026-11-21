<p align="center"><img src="docs/logo.svg" width="128" alt="LarpChanger logo"></p>

<h1 align="center">LarpChanger</h1>

<p align="center">A client-side CS2 skin changer for <b>offline use only</b>.</p>

> [!IMPORTANT]
> LarpChanger is intended for **offline use only**: local matches against bots, with CS2 started using `-insecure`.
> The DLL refuses to load unless `-insecure` is on CS2's command line, which keeps VAC-secured servers off the table.
> It contains no anti-cheat bypass. Do not try to use it online.

## What it does

Everything is cosmetic and only visible on your own PC.

- Weapon skins with wear, pattern, StatTrak, name tags, stickers and charms
- Knives and gloves
- Agents for both teams
- Music kits, including the MVP banner
- Medals / coins

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 or newer with the C++ workload
- LLVM (clang-cl). Set `OC_LLVM` to its `bin` folder if it isn't in `%USERPROFILE%\tools\llvm\bin` or `C:\Program Files\LLVM\bin`
- Node.js (for the menu UI)

## Build

```bat
cd src\ui\panorama
npm ci
cd ..\..\..
tools\build.cmd release
```

The output lands in `build\release\`: `larpchanger.dll` and `larpchanger_inject.exe`.
Use `tools\build.cmd dev` for a build that writes a log to `%APPDATA%\openchanger\dev\log.txt`.

## Use

1. Download `LarpChanger.exe` from [Releases](../../releases).
2. Start CS2 with the launch option `-insecure`.
3. Open `LarpChanger.exe` and press **Load into CS2**. It unpacks itself to `%LOCALAPPDATA%\LarpChanger`.
4. Open the Star of David tab in the main menu, pick your items, then start a match with bots.

Building from source also gives you `larpchanger_inject.exe`, a console loader that does the same thing.

## Credits

LarpChanger is a fork of [OpenChanger](https://github.com/realmaximg/OpenChanger) by realmaximg, used under the MIT License.
The installer uses [Dear ImGui](https://github.com/ocornut/imgui) (MIT). Third-party libraries keep their own licenses; see `thirdparty/` and `thirdparty/NOTICES.md`.

## License

MIT. See [LICENSE](LICENSE).
