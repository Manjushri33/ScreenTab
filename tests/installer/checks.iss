var
  ScenarioReport: String;

procedure AssertTrue(Condition: Boolean; Description: String);
begin
  if not Condition then RaiseException(Description);
end;

procedure CheckScenario(Registered, ExecutablePresent, StartupEnabled: Boolean;
  Version, ExpectedAction: String);
var
  OriginalTitle, OriginalWelcome, OriginalPageName, OriginalPageDescription: String;
begin
  FixtureRegistered := Registered;
  FixtureExecutablePresent := ExecutablePresent;
  FixtureVersion := Version;
  FixtureStartupEnabled := StartupEnabled;
  FixtureDirectory := 'D:\Custom install\ScreenTab';
  WizardForm.Caption := 'ScreenTab Setup';
  WizardForm.WelcomeLabel1.Caption := 'Welcome to ScreenTab Setup';
  WizardForm.WelcomeLabel2.Caption := 'Install ScreenTab';
  WizardForm.DirEdit.Text := 'D:\New installation';
  OriginalTitle := WizardForm.Caption;
  OriginalWelcome := WizardForm.WelcomeLabel1.Caption;
  ProductionInitializeWizard;
  ProductionCurPageChanged(wpWelcome);
  if ExpectedAction = '' then
  begin
    AssertTrue(not ProductionShouldSkipPage(wpWelcome), 'Fresh/stale install lost welcome');
    AssertTrue(not ProductionShouldSkipPage(wpSelectDir), 'Fresh/stale install lost folder selection');
    AssertTrue(not ProductionShouldSkipPage(wpSelectTasks), 'Fresh/stale install lost startup choice');
    AssertTrue(WizardForm.DirEdit.Text = 'D:\New installation', 'Fresh/stale destination changed');
    AssertTrue(WizardForm.Caption = OriginalTitle, 'Fresh/stale install title changed');
    AssertTrue(WizardForm.WelcomeLabel1.Caption = OriginalWelcome,
      'Fresh/stale install welcome changed');
    Exit;
  end;
  AssertTrue(ProductionShouldSkipPage(wpWelcome), 'Maintenance still shows setup welcome');
  AssertTrue(ProductionShouldSkipPage(wpSelectDir), 'Maintenance still asks for installation folder');
  AssertTrue(ProductionShouldSkipPage(wpSelectProgramGroup), 'Maintenance still asks for start-menu folder');
  AssertTrue(ProductionShouldSkipPage(wpSelectTasks), 'Maintenance still asks for startup preference');
  AssertTrue(not ProductionShouldSkipPage(wpReady), 'Maintenance skips update confirmation');
  AssertTrue(not ProductionShouldSkipPage(wpFinished), 'Maintenance skips completion');
  AssertTrue(WizardForm.DirEdit.Text = FixtureDirectory, 'Maintenance lost custom installed folder');
  AssertTrue(WizardIsTaskSelected('startup') = StartupEnabled,
    Version + ': maintenance changed startup setting, expected ' + IntToStr(Ord(StartupEnabled)));
  AssertTrue(Pos(ExpectedAction, WizardForm.Caption) > 0,
    Version + ': window title does not identify ' + ExpectedAction);
  AssertTrue(Pos(ExpectedAction, WizardForm.WelcomeLabel1.Caption) > 0,
    Version + ': welcome does not identify ' + ExpectedAction);
  if Version <> '' then
    AssertTrue(Pos(Version, WizardForm.WelcomeLabel2.Caption) > 0,
      'Welcome omits installed version');
  AssertTrue(Pos('{#MyAppVersion}', WizardForm.WelcomeLabel2.Caption) > 0,
    'Welcome omits incoming version');
  AssertTrue(Pos('closed', WizardForm.WelcomeLabel2.Caption) > 0,
    'Welcome omits running app shutdown');

  WizardForm.PageNameLabel.Caption := 'Select Destination Location';
  WizardForm.PageDescriptionLabel.Caption := 'Choose a folder';
  OriginalPageName := WizardForm.PageNameLabel.Caption;
  OriginalPageDescription := WizardForm.PageDescriptionLabel.Caption;
  ProductionCurPageChanged(wpSelectDir);
  AssertTrue(WizardForm.PageNameLabel.Caption = OriginalPageName,
    'Maintenance changed unrelated page heading');
  AssertTrue(WizardForm.PageDescriptionLabel.Caption = OriginalPageDescription,
    'Maintenance changed unrelated page description');
  WizardForm.ReadyLabel.Caption := 'Click Install to continue';
  WizardForm.ReadyMemo.Text := 'Destination location: D:\Custom install\ScreenTab';
  ProductionCurPageChanged(wpReady);
  AssertTrue(Pos(ExpectedAction, WizardForm.NextButton.Caption) > 0,
    Version + ': confirmation button does not identify ' + ExpectedAction);
  AssertTrue(Pos(ExpectedAction, WizardForm.PageNameLabel.Caption) > 0,
    'Ready heading omits maintenance action');
  if Version <> '' then
    AssertTrue(Pos(Version, WizardForm.PageDescriptionLabel.Caption) > 0,
      'Ready header omits installed version');
  AssertTrue(Pos('{#MyAppVersion}', WizardForm.PageDescriptionLabel.Caption) > 0,
    'Ready header omits incoming version');
  AssertTrue(Pos(ExpectedAction, WizardForm.ReadyLabel.Caption) > 0,
    'Ready instructions still say Install');
  AssertTrue(Pos('{#MyAppVersion}', WizardForm.ReadyMemo.Text) > 0,
    'Ready page omits incoming version');
  if Version <> '' then
    AssertTrue(Pos(Version, WizardForm.ReadyMemo.Text) > 0,
      'Ready page omits installed version');
  AssertTrue(Pos('D:\Custom install\ScreenTab', WizardForm.ReadyMemo.Text) > 0,
    'Ready page loses selected installation folder');
  ProductionCurPageChanged(wpInstalling);
  AssertTrue(Pos('Installing', WizardForm.PageNameLabel.Caption) = 0,
    'Maintenance progress still says Installing');
  ProductionCurPageChanged(wpFinished);
  AssertTrue(Pos(ExpectedAction, WizardForm.FinishedHeadingLabel.Caption) > 0,
    'Finished page omits maintenance action');
  AssertTrue(Pos('{#MyAppVersion}', WizardForm.FinishedLabel.Caption) > 0,
    'Finished page omits incoming version');
end;

procedure RunScenarioChecks;
begin
  CheckScenario(True, True, True, '0.1.1', 'Update');
  CheckScenario(True, True, False, '0.1.2', 'Update');
  CheckScenario(True, True, True, '{#MyAppVersion}', 'Reinstall');
  CheckScenario(True, True, False, '0.1.10', 'Downgrade');
  CheckScenario(True, True, False, '', 'Update');
  CheckScenario(False, False, False, '', '');
  CheckScenario(True, False, False, '0.1.1', '');
end;

procedure InitializeWizard;
begin
  FixtureRegistered := ExpandConstant('{param:FLOW}') <> 'fresh';
  FixtureExecutablePresent := FixtureRegistered;
  FixtureVersion := '0.1.1';
  FixtureDirectory := 'D:\Custom install\ScreenTab';
  FixtureStartupEnabled := ExpandConstant('{param:FLOW}') <> 'update-disabled';
  ProductionInitializeWizard;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := ProductionShouldSkipPage(PageID);
end;

procedure CurPageChanged(PageID: Integer);
begin
  ProductionCurPageChanged(PageID);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  { Silent Setup simulates the initial welcome click even when that page is skipped.
    Permit only that harmless transition; always stop before the install action. }
  if (CurPageID = wpWelcome) and ProductionShouldSkipPage(wpWelcome) then
  begin
    Result := True;
    Exit;
  end;
  { Verify the first page chosen by the real wizard engine, then cancel. }
  try
      if FixtureRegistered then
      begin
        AssertTrue(CurPageID = wpReady,
          'Actual update flow does not start at confirmation; first page ID ' + IntToStr(CurPageID));
        AssertTrue(Pos('Update', WizardForm.NextButton.Caption) > 0,
          'Actual first page does not have Update button');
        AssertTrue(Pos('0.1.1', WizardForm.PageDescriptionLabel.Caption) > 0,
          'Actual first page omits installed version');
        AssertTrue(Pos('{#MyAppVersion}', WizardForm.PageDescriptionLabel.Caption) > 0,
          'Actual first page omits incoming version');
        AssertTrue(Pos('D:\Custom install\ScreenTab', WizardForm.ReadyMemo.Text) > 0,
          'Actual first page omits preserved destination');
        AssertTrue(WizardIsTaskSelected('startup') = FixtureStartupEnabled,
          'Actual update flow changed startup setting');
        RunScenarioChecks;
        ScenarioReport := 'PASS: 7 installer scenarios; actual first page: update confirmation; ' +
          ExpandConstant('{param:FLOW}');
      end
      else
      begin
        AssertTrue(CurPageID = wpWelcome, 'Actual fresh flow does not start at welcome');
        ScenarioReport := 'PASS: actual first page: normal setup welcome.';
      end;
  except
    ScenarioReport := 'FAIL: ' + GetExceptionMessage;
  end;
  SaveStringToFile(ExpandConstant('{param:RESULTFILE}'), ScenarioReport, False);
  Result := False;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  { Independent guard: this executable must never perform an installation. }
  Result := 'Installer UI test: installation is prohibited.';
end;
