"""Run production installer Pascal code against controlled OS inputs.

The harness uses real Inno Setup wizard controls, writes no registry entries,
and always cancels before installation. Requires Windows and ISCC.exe.
"""

import argparse
import pathlib
import re
import subprocess
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]


def run(iscc):
    source = (ROOT / 'installer/ScreenTab.iss').read_text(encoding='utf-8-sig')
    sections = dict(re.findall(
        r'^\[([^\]]+)\]\s*\n(.*?)(?=^\[|\Z)', source, re.M | re.S))
    code = sections['Code']
    # Replace external inputs only; all decisions and captions remain production code.
    for original, replacement in (
        ('RegQueryStringValue', 'FixtureRegQueryStringValue'),
        ('FileExists', 'FixtureFileExists'),
        ('InitializeWizard', 'ProductionInitializeWizard'),
        ('CurPageChanged', 'ProductionCurPageChanged'),
        ('CurStepChanged', 'ProductionCurStepChanged'),
        ('PrepareToInstall', 'ProductionPrepareToInstall'),
        ('ShouldSkipPage', 'ProductionShouldSkipPage'),
    ):
        code = re.sub(r'\b' + original + r'\b', replacement, code)
    if not re.search(r'procedure\s+ProductionInitializeWizard\b', code, re.I):
        code += '\nprocedure ProductionInitializeWizard; begin end;\n'
    if not re.search(r'procedure\s+ProductionCurPageChanged\b', code, re.I):
        code += '\nprocedure ProductionCurPageChanged(PageID: Integer); begin end;\n'
    if not re.search(r'function\s+ProductionShouldSkipPage\b', code, re.I):
        code += '\nfunction ProductionShouldSkipPage(PageID: Integer): Boolean; begin Result := False; end;\n'

    fixtures = (ROOT / 'tests/installer/fixtures.iss').read_text(encoding='utf-8')
    checks = (ROOT / 'tests/installer/checks.iss').read_text(encoding='utf-8')
    harness = source.split('[Setup]', 1)[0] + '''
[Setup]
AppName=ScreenTab Installer Tests
AppVersion=1
DefaultDirName={tmp}\\ScreenTab-installer-tests
PrivilegesRequired=lowest
CreateAppDir=yes
Uninstallable=no
CreateUninstallRegKey=no
DisableWelcomePage=no
DisableDirPage=no
OutputBaseFilename=installer-tests
WizardStyle=modern
Compression=none
[Tasks]
''' + sections['Tasks'] + '''
[CustomMessages]
''' + sections.get('CustomMessages', '') + '\n[Code]\n' + fixtures + code + checks
    with tempfile.TemporaryDirectory(prefix='installer-', dir=ROOT / 'build') as temp:
        temp = pathlib.Path(temp)
        script = temp / 'tests.iss'
        script.write_text(harness, encoding='utf-8-sig')
        subprocess.run([str(iscc), '/Qp', f'/O{temp}', str(script)], check=True)
        for flow in ('update', 'update-disabled', 'fresh'):
            result = temp / f'{flow}.txt'
            log = temp / f'{flow}.log'
            execution = subprocess.run([
                str(temp / 'installer-tests.exe'), '/VERYSILENT', '/SUPPRESSMSGBOXES',
                '/NORESTART', '/SP-', f'/FLOW={flow}', f'/RESULTFILE={result}', f'/LOG={log}',
            ], timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
            if not result.exists():
                raise RuntimeError(log.read_text(errors='replace'))
            report = result.read_text(encoding='utf-8-sig')
            print(report.strip())
            (ROOT / f'build/installer-test-{flow}.log').write_bytes(log.read_bytes())
            # Returning False from NextButtonClick aborts silent setup before installation.
            if execution.returncode != 1 or not report.startswith('PASS:'):
                raise RuntimeError(f'Installer tests failed (exit {execution.returncode})')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iscc', type=pathlib.Path, required=True)
    run(parser.parse_args().iscc.resolve())
