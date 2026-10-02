var
  FixtureRegistered: Boolean;
  FixtureExecutablePresent: Boolean;
  FixtureVersion: String;
  FixtureDirectory: String;

function FixtureRegQueryStringValue(RootKey: Integer;
  SubKeyName, ValueName: String; var Value: String): Boolean;
begin
  Result := False;
  Value := '';
  if not FixtureRegistered then Exit;
  if RootKey <> HKLM64 then Exit;
  if SubKeyName <>
    'Software\Microsoft\Windows\CurrentVersion\Uninstall\{53AB2717-1562-4D2F-9716-1F055891A1F4}_is1' then Exit;
  if ValueName = 'InstallLocation' then
  begin
    Value := FixtureDirectory;
    Result := True;
  end
  else if ValueName = 'DisplayVersion' then
  begin
    Value := FixtureVersion;
    Result := Value <> '';
  end;
end;

function FixtureFileExists(Filename: String): Boolean;
begin
  Result := FixtureExecutablePresent and
    (Filename = AddBackslash(FixtureDirectory) + 'ScreenTab.exe');
end;
