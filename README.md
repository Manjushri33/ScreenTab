<div align="center">
  <img src="assets/app-icon.png?v=3" alt="ScreenTab icon" width="160" />

# ScreenTab

**Alt+Tab for the screen you're actually using.**

ScreenTab is a small open-source Windows 11 utility that keeps the native Alt+Tab interface and thumbnails, while showing only windows that belong to the monitor you're currently working on.

[![Build](https://github.com/Manjushri33/ScreenTab/actions/workflows/build.yml/badge.svg)](https://github.com/Manjushri33/ScreenTab/actions/workflows/build.yml)

</div>

## Features

- Keeps the native Windows 11 Alt+Tab UI and live thumbnails.
- Shows only windows from the monitor containing the foreground window.
- Falls back to the mouse cursor's monitor when needed.
- Keeps the Alt+Tab switcher on the active monitor.
- Works across repeated and rapid Alt+Tab invocations.
- Leaves `Win + Tab` / Task View unchanged.
- Runs quietly in the system tray.
- Includes **Pause**, **Start with Windows**, **About**, and **Exit**.
- Reconnects automatically after Explorer restarts.

## Download

ScreenTab is currently a **developer preview**. After the Windows build is validated, use:

**Releases → Latest → `ScreenTab-Setup.exe`**

Official releases also include `ScreenTab-Setup.exe.sha256` so the installer can be verified.

## Installation

1. Download `ScreenTab-Setup.exe` from the latest GitHub Release.
2. Double-click the installer.
3. If Windows SmartScreen appears because this early build is not code-signed, choose **More info → Run anyway** only if you downloaded it from this repository's official Releases page.
4. Complete the installer.
5. ScreenTab starts automatically and appears in the system tray.
6. **Start with Windows** is enabled by default.

After installation, use `Alt + Tab` normally. When you're working on one monitor, ScreenTab shows only windows from that monitor. Switch to another monitor and the Alt+Tab list follows you.

Right-click the tray icon for:

- **Pause** — temporarily restore normal Alt+Tab behavior.
- **Start with Windows** — enable or disable automatic startup.
- **About** — version and project information.
- **Exit** — close ScreenTab and immediately restore standard Windows Alt+Tab.

### Uninstall

Open **Windows Settings → Apps → Installed apps → ScreenTab → Uninstall**.

The uninstaller removes ScreenTab and its startup entry.

## Requirements

- Windows 11
- x64 PC
- Standard Windows 11 Alt+Tab

The first public version does not target Windows 10, ARM64, old ExplorerPatcher Alt+Tab modes, or third-party task switchers.

## How it works

ScreenTab uses a small native controller plus a hook DLL loaded into the current user's Explorer process. It filters the Windows shell application-view visibility path only while Alt+Tab is being built, so Windows continues to draw its own switcher and thumbnails.

Windows does not expose a supported public API for per-monitor native Alt+Tab filtering, so this relies on Windows shell internals that may change between builds. ScreenTab resolves the installed build's shell symbols at runtime and **fails open**: if a compatible symbol can't be resolved, it leaves standard Alt+Tab untouched.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for technical details.

## Build from source

Requirements:

- Windows 11 x64
- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.24+
- Python 3
- Inno Setup 6 to build the installer

```powershell
python -m pip install pillow
python scripts/make_icon.py
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

## Contributing

Bug reports are especially useful when they include the Windows 11 build number, whether the normal native Alt+Tab UI is enabled, and exact reproduction steps.

See [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Credits

The filtering strategy and relevant Windows shell symbol names were informed by the open-source Windhawk **Alt+Tab per monitor** mod by L3r0y and the Windhawk project by Ramen Software. ScreenTab is a separate standalone implementation and does not require Windhawk.

Third-party licenses and notices are listed in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## License

ScreenTab's own source is released under the [MIT License](LICENSE).
