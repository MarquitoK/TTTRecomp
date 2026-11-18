# TTTRecomp

A native Windows port of **Dragon Ball Z: Tenkaichi Tag Team** (PSP, ULUS10537), made by statically recompiling the original game code into C++.

TTTRecomp is not an emulator. The game's MIPS code is translated ahead of time into C++ and compiled into a regular Windows program, with a new runtime that replaces the PSP hardware: OpenGL renderer, audio, input, saves, and networking. Because the port runs as native code, it can go far beyond the original hardware: higher resolutions, 60 fps (experimental), online play, a full mod system, and a much larger roster.

> [!WARNING]
> **TTTRecomp is in early development (alpha).** It has had limited testing so far. Expect bugs, crashes, stutters, and performance problems on some PCs, especially with the experimental options turned on. Please report anything you find in [Issues](../../issues), including your PC specs (CPU, GPU, RAM) and the steps to reproduce it.

> **This repository contains no game files.** You need your own legally obtained copy of the game as an ISO. The port is built on your PC from your ISO, and nothing from the game is ever distributed here.

## Features

- **Native PC port:** any resolution, windowed or fullscreen, keyboard, mouse, and controller support, rumble, and Discord Rich Presence.
- **Online and LAN play:** casual and ranked rooms, with fair fixed rules in ranked.
- **Local multiplayer:** split screen, with an option to send player 2's view to a second monitor.
- **Mod system:** `.mkpak` mod packages, an in-game mod manager, and a built-in **mod library**. Modders sign in with GitHub, publish straight from the game, and players browse and install mods from the same menu.
- **Modding tools inside the game:** a mod creator, character and stage editors, a material editor, and a render editor, all in the *Developer* tab of the settings menu.
- **Expanded roster and stages:** room for characters and maps well beyond the original limits.
- **Optional HUD styles:** Original, Original (Full), and HUD mods such as Budokai Tenkaichi 3 and Sparking Zero style bars.
- **In-game settings menu** (F1, or Start + Select), drawn over the game in its own style and fully usable with a controller or mouse.
- **Experimental options:** 60 fps, two-phase charge (SZ), and a BT3-style camera. They are off by default and can be turned on in the settings menu.

## Requirements

- Windows 10 or 11, 64-bit.
- A GPU with OpenGL 3.3 support.
- About **8 GB** of free disk space.
- An internet connection for the first build (the setup downloads its own compiler and Python).
- Your own ISO of **Dragon Ball Z: Tenkaichi Tag Team (USA, ULUS10537)**. Other regions are not supported.

## Installing (players)

1. Download **`TTTRecomp-Setup-vX.Y.Z.exe`** from the [Releases](../../releases) page and run it. It is a single file: everything the port needs is inside.
2. Drop your ISO onto the window, or click to browse for it. The setup checks that it is the right game and version.
3. Choose your options: character colours (all or only the originals), everything unlocked from the start, install folder, and desktop shortcut. The port files, the generated C++ code, and the extracted assets all go into that install folder.
4. Press **Build**. The first build takes a while (around 20 to 30 minutes, depending on your PC). Your ISO is only read and is never changed.
5. When it finishes, press **Play**, or use the desktop shortcut or `launcher.exe` in the install folder.

### Updates

When a new release is out, the game offers to update when you start it (checked once a day), and the setup shows it on its first page. Updating only rebuilds what the new version changed, so it usually takes a few minutes, and it keeps your mods, saves, and settings. The assets are only extracted again when the extractor itself changes.

Running the setup again on the same folder also updates the port and keeps your mods and saves.

### Controls

| Key | Action |
| --- | --- |
| F1 / Start + Select | Settings menu |
| F11 | Toggle fullscreen |
| Shift + Esc | Quit |
| Ctrl + F11 / L1 + R1 + L3 | Send player 2's view to a second monitor (local split screen) |

You can remap game controls from the settings menu.

## Building from source (developers)

The setup is a front end for `tools/build_from_iso.py`. To build by hand you need:

- **Python 3.13** (standard library only).
- **MinGW-w64 GCC** (the setup uses WinLibs GCC 15.2.0, POSIX threads, UCRT), with `g++`, `windres`, and `make` or `mingw32-make` on `PATH`.

```
git clone https://github.com/MarquitoK/TTTRecomp.git
cd TTTRecomp
python tools/build_from_iso.py --iso "C:\path\to\game.iso" --root .
```

| Option | Meaning |
| --- | --- |
| `--iso PATH` | Your game ISO (required) |
| `--root DIR` | Folder where the port is built (default: the repo) |
| `--colors all\|original` | Extract all character colours or only the original ones |
| `--no-unlock` | Keep the original unlock progression |
| `--jobs N` | Parallel compile jobs |
| `--until STEP` | Stop after a given step |
| `--force` | Redo every step |

The pipeline:
1. Reads the ISO and takes out the EBOOT and its overlays.
2. Applies the port patches in `patchers/`.
3. Analyses and translates the code to C++ with `recompiler/`.
4. Compiles it together with `runtime/`.
5. Extracts the game assets to `assets/`.

Everything generated (`game/`, `generated/`, `assets/`) stays on your machine and is ignored by git.

To build the setup itself (or the release build with `python tools/make_release.py`, which writes the single-file setup with the port embedded to `dist/`):

```
windres tools/setup/setup.rc -O coff -o generated/setup_res.o
g++ -O2 -std=c++17 -municode -mwindows -static -o TTTRecomp-Setup.exe tools/setup/setup.cpp generated/setup_res.o -lopengl32 -lglu32 -lgdi32 -lwinhttp -lbcrypt -lole32 -luuid -lshell32 -lcomdlg32
```

Developer keys and diagnostics are only enabled when the environment variable `TTT_DEBUG=1` is set.

## Repository layout

| Folder | Contents |
| --- | --- |
| `recompiler/` | MIPS/VFPU analyser and C++ generator |
| `runtime/` | The PC runtime: boot, CPU helpers, GPU (OpenGL), audio, input, saves, networking, UI, and game patches |
| `patchers/` | Binary patches applied to the game code before translation |
| `games/ttt/` | Port data: fonts, layouts, port-made assets, and game tables |
| `tools/` | Build pipeline, setup, asset extraction, and modding utilities |
| `tests/` | Recompiler tests |

## Contributing

Issues and pull requests are welcome. Every pull request is reviewed and must be approved by the maintainer before it is merged. Please keep changes focused, and never include game files or anything extracted from the game.

## Legal

TTTRecomp is a fan project. It is not affiliated with, endorsed by, or sponsored by Bandai Namco Entertainment, Spike, Bird Studio, Shueisha, Toei Animation, or Sony. *Dragon Ball* and all related names and characters are trademarks of their respective owners. This project does not include any copyrighted game code or assets, and does not support piracy: please use a copy of the game you own.

## License

The TTTRecomp source code is released under the **GNU General Public License v3.0**. See [LICENSE](LICENSE).

Third-party components keep their own licenses:

| Component | Location | License |
| --- | --- | --- |
| libjuice | `runtime/net/juice/` | Mozilla Public License 2.0 (see `runtime/net/juice/LICENSE`) |
| stb_image | `runtime/third_party/stb_image.h` | Public domain or MIT (see the file header) |
| Source Sans 3 | `games/ttt/fonts/` | SIL Open Font License 1.1 (see `games/ttt/fonts/OFL_SourceSans3.txt`) |

The setup downloads [WinLibs GCC](https://winlibs.com/) and the [Python embeddable package](https://www.python.org/) from their official sources and checks their SHA-256 hashes. Neither is redistributed here.
