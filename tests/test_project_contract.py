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
            '#define MyAppVersion "0.1.4"',
            '#define MyAppPublisher "Manjushri33"',
            '#define MyAppURL "https://github.com/Manjushri33/ScreenTab"',
            'AppPublisher={#MyAppPublisher}',
            'AppPublisherURL={#MyAppURL}',
            'AppSupportURL={#MyAppURL}/issues',
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
        self.assertIn('project(ScreenTab VERSION 0.1.4', cmake)
        self.assertIn('configure_file(', cmake)
        self.assertIn('FileDescription', version_rc)
        self.assertIn('ScreenTab', version_rc)
        self.assertIn('CompanyName', version_rc)
        self.assertIn('Manjushri33', version_rc)

    def test_symbol_resolver_requires_exact_public_pdb_symbols(self):
        text = self.read('src/controller/symbols.cpp')
        self.assertIn('SYMOPT_PUBLICS_ONLY', text)
        self.assertIn('SymGetModuleInfoW64', text)
        self.assertIn('PdbUnmatched', text)
        self.assertIn('??_7CWin32ApplicationView@@6BIApplicationView@@@', text)
        self.assertIn('??_7CWinRTApplicationView@@6BIApplicationView@@@', text)
        self.assertIn('Missing symbols:', text)

    def test_symbol_runtime_is_in_portable_and_installer_packages(self):
        cmake = self.read('CMakeLists.txt')
        workflow = self.read('.github/workflows/build.yml')
        installer = self.read('installer/ScreenTab.iss')
        for file in ('dbghelp.dll', 'symsrv.dll', 'msdia140.dll'):
            self.assertIn(file, cmake)
            self.assertIn(file, workflow)
            self.assertIn(file, installer)

    def test_hook_checks_resolved_module_identity_and_reports_readiness(self):
        protocol = self.read('src/common/protocol.h')
        hook = self.read('src/hook/alt_tab_hook.cpp')
        dllmain = self.read('src/hook/dllmain.cpp')
        self.assertIn('ModuleIdentity', protocol)
        self.assertIn('hookStatus', protocol)
        self.assertIn('ModuleMatchesResolvedImage', hook)
        self.assertIn('ReportHookStatus', dllmain)
        self.assertNotIn('InterlockedExchange(&g_state->enabled,0)', hook)

    def test_controller_re_resolves_on_explorer_change_and_retries_failure(self):
        main = self.read('src/controller/main.cpp')
        self.assertIn('StartResolution', main)
        self.assertIn('WM_RESOLVED', main)
        self.assertIn('kResolveRetryMs', main)
        self.assertIn('hookStatus', main)
        self.assertIn('g_pendingPid', main)

    def test_injector_does_not_free_remote_path_while_load_is_running(self):
        injector = self.read('src/controller/injector.cpp')
        self.assertIn('wait == WAIT_OBJECT_0', injector)
        self.assertIn('if (wait == WAIT_OBJECT_0) VirtualFreeEx', injector)

    def test_uncertain_injection_is_tracked_and_stale_dll_is_not_reloaded(self):
        controller = self.read('src/controller/controller.h')
        injector = self.read('src/controller/injector.cpp')
        main = self.read('src/controller/main.cpp')
        self.assertIn('enum class InjectionResult', controller)
        self.assertIn('InjectionResult::Pending', injector)
        self.assertIn('InjectionResult::Pending', main)
        self.assertIn('ModuleLookup::Found', injector)
        self.assertIn('ModuleLookup::Error', injector)
        self.assertIn('kHookStartupTimeoutMs', main)

    def test_hook_handshake_waits_for_init_thread_to_exit(self):
        protocol = self.read('src/common/protocol.h')
        dllmain = self.read('src/hook/dllmain.cpp')
        main = self.read('src/controller/main.cpp')
        self.assertIn('hookInitThreadId', protocol)
        self.assertIn('hookInitThreadId', dllmain)
        self.assertIn('HookInitThreadFinished', main)
        self.assertIn('FreeLibraryAndExitThread', dllmain)
        self.assertIn('if (!thread) return FALSE;', dllmain)

    def test_tray_icon_returns_after_explorer_restart(self):
        main = self.read('src/controller/main.cpp')
        self.assertIn('RegisterWindowMessageW(L"TaskbarCreated")', main)
        self.assertIn('AddTrayIcon(hwnd)', main)

if __name__ == '__main__':
    unittest.main()
