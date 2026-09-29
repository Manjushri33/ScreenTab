#define MyAppName "ScreenTab"
#define MyAppVersion "0.1.0"
#define MyAppExeName "ScreenTab.exe"

[Setup]
AppId={{53AB2717-1562-4D2F-9716-1F055891A1F4}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
DefaultDirName={autopf}\ScreenTab
UninstallDisplayName={#MyAppName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=ScreenTab-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\assets\app-icon.ico
UninstallDisplayIcon={app}\ScreenTab.exe

[Files]
Source: "..\build\Release\ScreenTab.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\ScreenTabHook.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\ScreenTab"; Filename: "{app}\ScreenTab.exe"

[Run]
Filename: "{app}\ScreenTab.exe"; Parameters: "--install-startup"; Flags: nowait postinstall runhidden runasoriginaluser

[UninstallRun]
Filename: "{cmd}"; Parameters: "/C reg delete HKCU\Software\Microsoft\Windows\CurrentVersion\Run /v ScreenTab /f"; Flags: runhidden
