import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class ProjectContractTests(unittest.TestCase):
    def read(self, rel):
        return (ROOT / rel).read_text(encoding='utf-8')

    def test_expected_project_files_exist(self):
        expected = [
            'CMakeLists.txt',
            'src/common/protocol.h',
            'src/controller/main.cpp',
            'src/controller/symbols.cpp',
            'src/controller/injector.cpp',
            'src/controller/app.rc',
            'src/controller/version.rc.in',
            'src/hook/dllmain.cpp',
            'src/hook/alt_tab_hook.cpp',
            'installer/ScreenTab.iss',
            'README.md',
        ]
        missing = [p for p in expected if not (ROOT / p).exists()]
        self.assertEqual(missing, [])

    def test_hook_has_repeated_alt_tab_guard_and_active_monitor_logic(self):
        text = self.read('src/hook/alt_tab_hook.cpp')
        self.assertIn('kDeltaThresholdMs', text)
        self.assertIn('GetForegroundWindow', text)
        self.assertIn('MonitorFromWindow', text)
        self.assertIn('CVirtualDesktop_IsViewVisible_Hook', text)

    def test_no_window_style_hiding_hack_remains(self):
        hook = self.read('src/hook/alt_tab_hook.cpp')
        self.assertNotIn('WS_EX_TOOLWINDOW', hook)
        self.assertNotIn('SetWindowLong', hook)

    def test_task_view_is_not_globally_filtered(self):
        text = self.read('src/hook/alt_tab_hook.cpp')
        self.assertIn('IsAltTabFilterWindowActive()', text)
        self.assertIn('return originalResult;', text)

    def test_controller_supports_pause_autostart_and_clean_unload(self):
        main = self.read('src/controller/main.cpp')
        injector = self.read('src/controller/injector.cpp')
        self.assertIn('Start with Windows', main)
        self.assertIn('Pause', main)
        self.assertIn('About', main)
        self.assertIn('Exit', main)
        self.assertIn('UninjectLibrary', injector)
        self.assertIn('UninjectLibrary', main)

    def test_installer_is_registered_customizable_and_branded(self):
        text = self.read('installer/ScreenTab.iss')
        for expected in [
            '#define MyAppVersion "0.1.1"',
            'AppPublisher=Manjushri33',
            'AppPublisherURL=https://github.com/Manjushri33/ScreenTab',
            'AppSupportURL=https://github.com/Manjushri33/ScreenTab/issues',
            'CreateUninstallRegKey=yes',
            'Uninstallable=yes',
            'DisableDirPage=no',
            'SetupIconFile=..\\assets\\app-icon.ico',
            'UninstallDisplayIcon={app}\\ScreenTab.exe',
            'CloseApplications=yes',
            '[Tasks]',
            'Start ScreenTab with Windows',
            'Launch ScreenTab',
        ]:
            self.assertIn(expected, text)

    def test_version_metadata_is_embedded(self):
        cmake = self.read('CMakeLists.txt')
        version_rc = self.read('src/controller/version.rc.in')
        self.assertIn('project(ScreenTab VERSION 0.1.1', cmake)
        self.assertIn('configure_file(', cmake)
        self.assertIn('FileDescription', version_rc)
        self.assertIn('ScreenTab', version_rc)
        self.assertIn('CompanyName', version_rc)
        self.assertIn('Manjushri33', version_rc)

    def test_symbol_resolver_matches_stable_identifiers_and_reports_missing_symbols(self):
        text = self.read('src/controller/symbols.cpp')
        self.assertIn('CWin32ApplicationView::v_GetNativeWindow', text)
        self.assertIn('ITaskGroupWindowInformation', text)
        self.assertIn('::Position', text)
        self.assertNotIn('ITaskGroupWindowInformation>::Position', text)
        self.assertIn('Missing symbols:', text)

if __name__ == '__main__':
    unittest.main()
