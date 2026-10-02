procedure AssertTrue(Condition: Boolean; Description: String);
begin
  if not Condition then RaiseException(Description);
end;

procedure CheckScenario(Registered, ExecutablePresent: Boolean;
  Version, ExpectedAction: String);
var
  OriginalTitle, OriginalWelcome, OriginalPageName, OriginalPageDescription: String;
begin
  FixtureRegistered := Registered;
  FixtureExecutablePresent := ExecutablePresent;
  FixtureVersion := Version;
  FixtureDirectory := 'D:\Custom install\ScreenTab';
  WizardForm.Caption := 'ScreenTab Setup';
  WizardForm.WelcomeLabel1.Caption := 'Welcome to ScreenTab Setup';
  WizardForm.WelcomeLabel2.Caption := 'Install ScreenTab';
  OriginalTitle := WizardForm.Caption;
  OriginalWelcome := WizardForm.WelcomeLabel1.Caption;
  ProductionInitializeWizard;
  ProductionCurPageChanged(wpWelcome);
  if ExpectedAction = '' then
  begin
    AssertTrue(WizardForm.Caption = OriginalTitle, 'Fresh/stale install title changed');
    AssertTrue(WizardForm.WelcomeLabel1.Caption = OriginalWelcome,
      'Fresh/stale install welcome changed');
    Exit;
  end;
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

procedure InitializeWizard;
var
  Report: String;
begin
  try
    CheckScenario(True, True, '0.1.1', 'Update');
    CheckScenario(True, True, '0.1.2', 'Update');
    CheckScenario(True, True, '{#MyAppVersion}', 'Reinstall');
    CheckScenario(True, True, '0.1.10', 'Downgrade');
    CheckScenario(True, True, '', 'Update');
    CheckScenario(False, False, '', '');
    CheckScenario(True, False, '0.1.1', '');
    Report := 'PASS: 7 installer scenarios, using production Pascal code and real wizard controls.';
  except
    Report := 'FAIL: ' + GetExceptionMessage;
  end;
  SaveStringToFile(ExpandConstant('{param:RESULTFILE}'), Report, False);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  { Never install anything: this harness only evaluates UI behavior. }
  Result := False;
end;
