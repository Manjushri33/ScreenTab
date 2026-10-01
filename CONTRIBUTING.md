# Contributing to ScreenTab

Thanks for helping improve ScreenTab.

## Bug reports

Please include:

- Windows 11 version and OS build (`winver`);
- number and arrangement of monitors;
- exact steps to reproduce the problem;
- what you expected to happen;
- what happened instead;
- the exact tray status and any detail shown when you select it;
- the ScreenTab version and whether you used the installer or portable ZIP;
- whether ExplorerPatcher or another shell/task-switcher modification is installed.

## Building

ScreenTab targets Windows 11 x64 and is built with Visual Studio 2022 (Desktop development with C++), CMake 3.24+, and Python 3. The pinned Microsoft symbol runtime packages are downloaded and verified before configuration. Inno Setup 6 is needed only for the installer.

```powershell
python -m pip install pillow
python scripts/make_icon.py
.\scripts\fetch_symbol_runtime.ps1
cmake -S . -B build -A x64
cmake --build build --config Release
```

Run the contract tests with:

```powershell
python -m unittest discover -s tests -v
```

## Pull requests

Keep changes focused. For behavior changes, explain how you tested repeated Alt+Tab switching on at least two monitors and confirm that `Win + Tab` still behaves normally.
