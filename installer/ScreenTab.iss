#define MyAppName "ScreenTab"
#define MyAppVersion "0.1.1"
#define MyAppPublisher "Manjushri33"
#define MyAppURL "https://github.com/Manjushri33/ScreenTab"
#define MyAppExeName "ScreenTab.exe"

[Setup]
AppId={{53AB2717-1562-4D2F-9716-1F055891A1F4}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
VersionInfoVersion=0.1.1.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=ScreenTab installer
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
DefaultDirName={autopf}\ScreenTab
DisableDirPage=no
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\ScreenTab.exe
CreateUninstallRegKey=yes
Uninstallable=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=ScreenTab-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\assets\app-icon.ico
CloseApplications=yes
RestartApplications=no
CloseApplicationsFilter=ScreenTab.exe

[Files]
Source: "..\build\Release\ScreenTab.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\ScreenTabHook.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\dbghelp.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\symsrv.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\msdia140.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Tasks]
Name: "startup"; Description: "Start ScreenTab with Windows"; GroupDescription: "Startup:"; Flags: checkedonce

[Icons]
Name: "{group}\ScreenTab"; Filename: "{app}\ScreenTab.exe"; IconFilename: "{app}\ScreenTab.exe"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "ScreenTab"; ValueData: """{app}\ScreenTab.exe"" --startup"; Tasks: startup; Flags: uninsdeletevalue

[Run]
Filename: "{app}\ScreenTab.exe"; Description: "Launch ScreenTab"; Flags: nowait postinstall skipifsilent runasoriginaluser

[UninstallRun]
Filename: "{app}\ScreenTab.exe"; Parameters: "--shutdown"; Flags: runhidden waituntilterminated

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\ScreenTab\symbols"

[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Result := '';
  if FileExists(ExpandConstant('{app}\ScreenTab.exe')) then
  begin
    Exec(ExpandConstant('{app}\ScreenTab.exe'), '--shutdown', '', SW_HIDE,
      ewWaitUntilTerminated, ResultCode);
    Sleep(1000);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if not WizardIsTaskSelected('startup') then
      RegDeleteValue(HKEY_CURRENT_USER,
        'Software\Microsoft\Windows\CurrentVersion\Run', 'ScreenTab');
  end;
end;
