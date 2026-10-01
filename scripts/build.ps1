$ErrorActionPreference = 'Stop'
& "$PSScriptRoot\fetch_symbol_runtime.ps1"
cmake -S . -B build -A x64
cmake --build build --config Release
Write-Host "Built ScreenTab.exe, ScreenTabHook.dll, and Microsoft symbol runtime in build/Release"
