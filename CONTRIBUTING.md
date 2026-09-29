# Contributing to ScreenTab

Thanks for helping improve ScreenTab.

## Bug reports

Please include:

- Windows 11 version and OS build (`winver`);
- number and arrangement of monitors;
- exact steps to reproduce the problem;
- what you expected to happen;
- what happened instead;
- whether ExplorerPatcher or another shell/task-switcher modification is installed.

## Building

ScreenTab targets Windows 11 x64 and is built with Visual Studio 2022 + CMake.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Run the contract tests with:

```powershell
python -m unittest discover -s tests -v
```

## Pull requests

Keep changes focused. For behavior changes, explain how you tested repeated Alt+Tab switching on at least two monitors and confirm that `Win + Tab` still behaves normally.
