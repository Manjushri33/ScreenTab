#define MyAppName "ScreenTab"
#define MyAppVersion "0.1.3"
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
VersionInfoVersion=0.1.3.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=ScreenTab installer
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
DefaultDirName={autopf}\ScreenTab
DisableDirPage=no
DisableWelcomePage=no
UsePreviousAppDir=yes
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

[CustomMessages]
UpdateAction=Update
ReinstallAction=Reinstall
DowngradeAction=Downgrade
UpdateProgress=Updating ScreenTab
ReinstallProgress=Reinstalling ScreenTab
DowngradeProgress=Downgrading ScreenTab
ExistingVersion=ScreenTab %1 is already installed.
ExistingUnknownVersion=ScreenTab is already installed.
MaintenanceWelcome=%1%n%nThis wizard will %2 ScreenTab to version %3.%n%nThe running app will be closed before its files are replaced.
MaintenanceReadyTitle=Ready to %1
MaintenanceReadyDescription=Review the version change and installation settings.
MaintenanceReadyInstructions=Click %1 to %2 ScreenTab to version %3.
VersionChange=Version: %1 -> %2
UnknownVersionChange=Target version: %1
MaintenanceProgressDescription=Please wait while the ScreenTab files are replaced.
MaintenanceFinishedTitle=ScreenTab %1 Complete
MaintenanceFinishedDescription=ScreenTab %1 is ready to use.%n%nClick Finish to close this wizard.

[Code]
const
  ScreenTabUninstallKey =
    'Software\Microsoft\Windows\CurrentVersion\Uninstall\{53AB2717-1562-4D2F-9716-1F055891A1F4}_is1';

var
  InstalledVersion: String;
  MaintenanceAction: String;

procedure InitializeWizard;
var
  InstalledDirectory, ExistingDescription, ActionCaption: String;
  PreviousVersion, IncomingVersion: Int64;
  Comparison: Integer;
begin
  InstalledVersion := '';
  MaintenanceAction := '';
  if not RegQueryStringValue(HKLM64, ScreenTabUninstallKey,
    'InstallLocation', InstalledDirectory) then Exit;
  if not FileExists(AddBackslash(InstalledDirectory) + '{#MyAppExeName}') then Exit;

  RegQueryStringValue(HKLM64, ScreenTabUninstallKey,
    'DisplayVersion', InstalledVersion);
  MaintenanceAction := 'Update';
  if StrToVersion(InstalledVersion, PreviousVersion) and
    StrToVersion('{#MyAppVersion}', IncomingVersion) then
  begin
    Comparison := ComparePackedVersion(PreviousVersion, IncomingVersion);
    if Comparison = 0 then MaintenanceAction := 'Reinstall'
    else if Comparison > 0 then MaintenanceAction := 'Downgrade';
  end;

  ActionCaption := CustomMessage(MaintenanceAction + 'Action');
  WizardForm.Caption := '{#MyAppName} ' + ActionCaption;
  WizardForm.WelcomeLabel1.Caption := ActionCaption + ' {#MyAppName}';
  if InstalledVersion <> '' then
    ExistingDescription := FmtMessage(CustomMessage('ExistingVersion'), [InstalledVersion])
  else
    ExistingDescription := CustomMessage('ExistingUnknownVersion');
  WizardForm.WelcomeLabel2.Caption := FmtMessage(CustomMessage('MaintenanceWelcome'), [
    ExistingDescription, Lowercase(ActionCaption), '{#MyAppVersion}']);
end;

procedure CurPageChanged(CurPageID: Integer);
var
  ActionCaption, VersionDescription: String;
begin
  if MaintenanceAction = '' then Exit;
  ActionCaption := CustomMessage(MaintenanceAction + 'Action');
  case CurPageID of
    wpReady:
      begin
        WizardForm.PageNameLabel.Caption :=
          FmtMessage(CustomMessage('MaintenanceReadyTitle'), [ActionCaption]);
        WizardForm.PageDescriptionLabel.Caption := CustomMessage('MaintenanceReadyDescription');
        WizardForm.NextButton.Caption := '&' + ActionCaption;
        WizardForm.ReadyLabel.Caption := FmtMessage(CustomMessage('MaintenanceReadyInstructions'), [
          ActionCaption, Lowercase(ActionCaption), '{#MyAppVersion}']);
        if InstalledVersion <> '' then
          VersionDescription := FmtMessage(CustomMessage('VersionChange'), [
            InstalledVersion, '{#MyAppVersion}'])
        else
          VersionDescription := FmtMessage(CustomMessage('UnknownVersionChange'), ['{#MyAppVersion}']);
        WizardForm.ReadyMemo.Text := VersionDescription + #13#10#13#10 + WizardForm.ReadyMemo.Text;
      end;
    wpInstalling:
      begin
        WizardForm.PageNameLabel.Caption := CustomMessage(MaintenanceAction + 'Progress');
        WizardForm.PageDescriptionLabel.Caption := CustomMessage('MaintenanceProgressDescription');
      end;
    wpFinished:
      begin
        WizardForm.FinishedHeadingLabel.Caption :=
          FmtMessage(CustomMessage('MaintenanceFinishedTitle'), [ActionCaption]);
        WizardForm.FinishedLabel.Caption :=
          FmtMessage(CustomMessage('MaintenanceFinishedDescription'), ['{#MyAppVersion}']);
      end;
  end;
end;

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
