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

    def test_controller_supports_pause_and_autostart(self):
        text = self.read('src/controller/main.cpp')
        self.assertIn('Start with Windows', text)
        self.assertIn('Pause', text)
        self.assertIn('About', text)
        self.assertIn('Exit', text)

    def test_installer_enables_startup_behavior(self):
        text = self.read('installer/ScreenTab.iss')
        self.assertIn('ScreenTab.exe', text)
        self.assertIn('runhidden', text)

    def test_symbol_resolver_matches_stable_identifiers_and_reports_missing_symbols(self):
        text = self.read('src/controller/symbols.cpp')
        # DbgHelp's undecorated output can vary between Windows/PDB versions.
        # Matching should use stable class/function identifiers rather than one
        # complete demangled signature.
        self.assertIn('CWin32ApplicationView::v_GetNativeWindow', text)
        self.assertIn('ITaskGroupWindowInformation', text)
        self.assertIn('::Position', text)
        self.assertNotIn('ITaskGroupWindowInformation>::Position', text)
        # A failed lookup must say which symbols were missing, so a new Windows
        # build can be diagnosed without guessing.
        self.assertIn('Missing symbols:', text)

if __name__ == '__main__':
    unittest.main()
