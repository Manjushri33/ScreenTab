<div align="center">
  <img src="assets/app-icon.png?v=3" alt="ScreenTab icon" width="160" />

# ScreenTab

**Alt+Tab for the screen you're actually using.**

ScreenTab is a small open-source Windows 11 utility that keeps the native Alt+Tab interface and thumbnails while showing only windows that belong to the monitor you're currently working on.

[![Build](https://github.com/Manjushri33/ScreenTab/actions/workflows/build.yml/badge.svg)](https://github.com/Manjushri33/ScreenTab/actions/workflows/build.yml)

</div>

## Features

- Native Windows 11 Alt+Tab UI and live thumbnails.
- Only windows from the active monitor.
- Alt+Tab switcher follows the active monitor.
- Repeated and rapid Alt+Tab switching.
- `Win + Tab` / Task View stays unchanged.
- Small system-tray controller with **Pause**, **Start with Windows**, **About**, and **Exit**.
- Automatically reconnects after Explorer restarts.

## Install

### Installer — recommended

1. Open **Releases** and download `ScreenTab-Setup.exe` from the latest validated release.
2. Run the installer.
3. Choose the installation folder if you don't want the default `C:\Program Files\ScreenTab` location.
4. Leave **Start ScreenTab with Windows** enabled if you want ScreenTab to start automatically.
5. Leave **Launch ScreenTab** enabled to start it after installation.

If Windows SmartScreen appears because an early build is not code-signed, use **More info → Run anyway** only when the file came from this repository's official Releases page.

ScreenTab appears in **Windows Settings → Apps → Installed apps**, where it can be uninstalled normally.

### Portable

Download `ScreenTab-windows-x64.zip`, extract it to a permanent folder, keep all included DLLs beside `ScreenTab.exe`, and run `ScreenTab.exe`.

Portable mode does not create an Installed Apps entry. Use the tray menu's **Start with Windows** option if you want autostart.

## Use

Use `Alt + Tab` normally. ScreenTab keeps the native Windows switcher but filters it to the monitor you're currently using.

Right-click the tray icon for:

- **Pause** — temporarily restore standard Alt+Tab behavior.
- **Start with Windows** — toggle automatic startup.
- **About** — show version information.
- **Exit** — close ScreenTab and restore standard Alt+Tab.

## Uninstall

For an installer build, open **Windows Settings → Apps → Installed apps → ScreenTab → Uninstall**. The uninstaller closes ScreenTab, removes its startup entry, unloads its Explorer hook, and removes the symbol cache.

For portable mode, choose **Exit**, disable **Start with Windows** if it was enabled, and delete the extracted folder.

## Requirements

- Windows 11
- x64 PC
- Standard Windows 11 Alt+Tab
- Internet access on first run and after Windows updates to obtain matching Microsoft symbols

Windows 10, ARM64, old ExplorerPatcher Alt+Tab modes, and third-party task switchers are not targeted by the first release series.

## How it works

ScreenTab uses a small native controller plus a hook DLL loaded into the current user's Explorer process. It filters the Windows shell application-view visibility path only while Alt+Tab is being built, so Windows continues to draw its own switcher and thumbnails.

Windows doesn't expose a supported public API for per-monitor native Alt+Tab filtering. ScreenTab therefore resolves the installed Windows build's shell symbols at runtime and **fails open**: if the required symbols can't be resolved, standard Alt+Tab remains unchanged.

The portable ZIP and installer include Microsoft's DbgHelp symbol runtime. If the matching `twinui.pcshell.pdb` is unavailable from Microsoft's symbol server, ScreenTab leaves Alt+Tab unchanged and reports the problem from its tray icon.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for technical details.

## Build from source

Requirements:

- Windows 11 x64
- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.24+
- Python 3
- Inno Setup 6

```powershell
python -m pip install pillow
python scripts/make_icon.py
.\scripts\fetch_symbol_runtime.ps1
cmake -S . -B build -A x64
cmake --build build --config Release
```

Build the installer with:

```powershell
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\ScreenTab.iss
```

Or run:

```powershell
.\scripts\build.ps1
```

The symbol runtime fetch script downloads pinned Microsoft NuGet packages, verifies their SHA-256 hashes, and places the x64 DLLs in `build\symbol-runtime` for CMake to package.

## Contributing

Bug reports are especially useful when they include the Windows 11 build number and exact reproduction steps. See [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Credits

The filtering strategy and relevant Windows shell symbol names were informed by the open-source Windhawk **Alt+Tab per monitor** mod by L3r0y and the Windhawk project by Ramen Software. ScreenTab is a separate standalone implementation and does not require Windhawk.

Third-party licenses and notices are listed in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## License

ScreenTab's own source is released under the [MIT License](LICENSE).
