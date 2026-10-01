#include "controller/controller.h"
#include "controller/resource.h"
#include <shellapi.h>
#include <shlwapi.h>
#include <memory>
#include <string>
#include <thread>

namespace {
constexpr UINT WM_TRAY = WM_APP + 10;
constexpr UINT WM_RESOLVED = WM_APP + 11;
constexpr UINT ID_PAUSE = 1001;
constexpr UINT ID_STARTUP = 1002;
constexpr UINT ID_ABOUT = 1003;
constexpr UINT ID_EXIT = 1004;
constexpr UINT ID_STATUS = 1005;
constexpr UINT_PTR ID_TIMER = 42;
constexpr ULONGLONG kResolveRetryMs = 5 * 60 * 1000;
constexpr ULONGLONG kHookStartWarningMs = 15 * 1000;
constexpr ULONGLONG kHookStartupTimeoutMs = 60 * 1000;
constexpr ULONGLONG kRetryOnExplorerRestart = static_cast<ULONGLONG>(-1);

struct ResolveResult {
    DWORD explorerPid{};
    mat::SymbolOffsets symbols{};
    mat::ModuleIdentity identity{};
    std::wstring error;
    bool success{};
};

HANDLE g_mapping = nullptr;
mat::SharedState* g_state = nullptr;
UINT g_taskbarCreatedMessage = 0;
DWORD g_explorerPid = 0;
DWORD g_activePid = 0;
DWORD g_pendingPid = 0;
ULONGLONG g_pendingSince = 0;
ULONGLONG g_nextResolveTick = 0;
bool g_resolving = false;
bool g_pendingWarned = false;
bool g_pendingTimedOut = false;
bool g_paused = false;
std::wstring g_status = L"Starting";
std::wstring g_lastProblem;

void AddTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid{sizeof(nid)};
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = LoadIconW(GetModuleHandleW(nullptr),
                          MAKEINTRESOURCEW(IDI_APPICON));
    const std::wstring tip = L"ScreenTab - " +
        (g_paused ? std::wstring(L"Paused") : g_status);
    wcsncpy_s(nid.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

bool IsStartWithWindowsEnabled() {
    HKEY key{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_READ, &key) != ERROR_SUCCESS) return false;
    wchar_t value[32768]{};
    DWORD type = 0, size = sizeof(value);
    const bool enabled = RegQueryValueExW(
        key, L"ScreenTab", nullptr, &type,
        reinterpret_cast<BYTE*>(value), &size) == ERROR_SUCCESS;
    RegCloseKey(key);
    return enabled;
}

void SetStartWithWindows(bool enabled) {
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        0, nullptr, 0, KEY_SET_VALUE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS) return;
    if (enabled) {
        wchar_t exe[32768]{};
        GetModuleFileNameW(nullptr, exe, 32768);
        const std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --startup";
        RegSetValueExW(key, L"ScreenTab", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(cmd.c_str()),
                       static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, L"ScreenTab");
    }
    RegCloseKey(key);
}

void SetTrayStatus(HWND hwnd, const std::wstring& status) {
    g_status = status;
    NOTIFYICONDATAW nid{sizeof(nid)};
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_TIP;
    const std::wstring tip = L"ScreenTab - " +
        (g_paused ? std::wstring(L"Paused") : status);
    wcsncpy_s(nid.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void Balloon(HWND hwnd, const std::wstring& message) {
    NOTIFYICONDATAW nid{sizeof(nid)};
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    wcsncpy_s(nid.szInfoTitle, L"ScreenTab", _TRUNCATE);
    wcsncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
    nid.dwInfoFlags = NIIF_WARNING;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void ReportProblem(HWND hwnd, const std::wstring& message,
                   const std::wstring& status) {
    SetTrayStatus(hwnd, status);
    if (message != g_lastProblem) {
        g_lastProblem = message;
        Balloon(hwnd, message + L" Standard Alt+Tab is unchanged.");
    }
}

void StartResolution(HWND hwnd, DWORD pid) {
    if (!pid || g_resolving) return;
    g_resolving = true;
    SetTrayStatus(hwnd, L"Checking Windows symbols");
    try {
        std::thread([hwnd, pid] {
            auto result = std::make_unique<ResolveResult>();
            result->explorerPid = pid;
            result->success = mat::ResolveTwinuiSymbols(
                result->symbols, result->identity, result->error);
            ResolveResult* completed = result.release();
            if (!PostMessageW(hwnd, WM_RESOLVED, 0,
                              reinterpret_cast<LPARAM>(completed))) {
                delete completed;
            }
        }).detach();
    } catch (...) {
        g_resolving = false;
        g_nextResolveTick = GetTickCount64() + kResolveRetryMs;
        ReportProblem(hwnd, L"Could not start the symbol resolver",
                      L"Waiting to retry");
    }
}

bool HookInitThreadFinished(DWORD pid) {
    const DWORD tid = g_state->hookInitThreadId;
    if (!tid) return false;
    HANDLE thread = OpenThread(SYNCHRONIZE | THREAD_QUERY_LIMITED_INFORMATION,
                               FALSE, tid);
    if (!thread) return GetLastError() == ERROR_INVALID_PARAMETER;
    const DWORD owner = GetProcessIdOfThread(thread);
    const DWORD wait = WaitForSingleObject(thread, 0);
    CloseHandle(thread);
    return owner != 0 && (owner != pid || wait == WAIT_OBJECT_0);
}

void EnsureInjected(HWND hwnd) {
    if (!g_state) return;
    const DWORD pid = mat::FindExplorerProcessId();
    const ULONGLONG now = GetTickCount64();
    if (pid != g_explorerPid) {
        InterlockedExchange(&g_state->enabled, 0);
        g_explorerPid = pid;
        g_activePid = 0;
        g_pendingPid = 0;
        g_pendingWarned = false;
        g_pendingTimedOut = false;
        g_nextResolveTick = 0;
        g_state->explorerPid = pid;
        g_state->hookInitThreadId = 0;
        InterlockedExchange(&g_state->symbolsReady, 0);
        InterlockedExchange(&g_state->hookStatus,
                            static_cast<LONG>(mat::HookStatus::Pending));
        if (!pid) SetTrayStatus(hwnd, L"Waiting for Explorer");
    }
    if (!pid) return;

    if (g_pendingPid == pid) {
        const LONG status = InterlockedCompareExchange(&g_state->hookStatus, 0, 0);
        const bool reportedAndFinished =
            status != static_cast<LONG>(mat::HookStatus::Pending) &&
            HookInitThreadFinished(pid);
        if (reportedAndFinished &&
            status == static_cast<LONG>(mat::HookStatus::Ready)) {
            g_pendingPid = 0;
            g_activePid = pid;
            InterlockedExchange(&g_state->enabled, g_paused ? 0 : 1);
            g_lastProblem.clear();
            SetTrayStatus(hwnd, L"Working");
        } else if (reportedAndFinished &&
                   (status == static_cast<LONG>(mat::HookStatus::ModuleMismatch) ||
                    status == static_cast<LONG>(mat::HookStatus::InstallFailed))) {
            g_pendingPid = 0;
            InterlockedExchange(&g_state->symbolsReady, 0);
            g_nextResolveTick = status ==
                static_cast<LONG>(mat::HookStatus::ModuleMismatch)
                ? kRetryOnExplorerRestart : now + kResolveRetryMs;
            const std::wstring reason = status ==
                static_cast<LONG>(mat::HookStatus::ModuleMismatch)
                ? L"Explorer loaded a different Windows shell image. Restart Explorer or Windows"
                : L"Explorer could not install the Alt+Tab hooks";
            ReportProblem(hwnd, reason, L"Hook unavailable");
        } else if (!g_pendingTimedOut &&
                   now - g_pendingSince >= kHookStartupTimeoutMs) {
            g_pendingTimedOut = true;
            InterlockedExchange(&g_state->enabled, 0);
            ReportProblem(hwnd,
                          L"Hook startup did not finish. Restart Explorer or Windows",
                          L"Hook unavailable");
        } else if (!g_pendingWarned &&
                   now - g_pendingSince >= kHookStartWarningMs) {
            g_pendingWarned = true;
            InterlockedExchange(&g_state->enabled, 0);
            ReportProblem(hwnd, L"Explorer has not confirmed hook startup yet",
                          L"Waiting for hook");
        }
        return;
    }
    if (g_activePid == pid || g_resolving) return;

    if (!InterlockedCompareExchange(&g_state->symbolsReady, 0, 0)) {
        if (now >= g_nextResolveTick) StartResolution(hwnd, pid);
        return;
    }
    if (now < g_nextResolveTick) return;

    g_state->explorerPid = pid;
    g_state->hookInitThreadId = 0;
    InterlockedExchange(&g_state->hookStatus,
                        static_cast<LONG>(mat::HookStatus::Pending));
    std::wstring error;
    const auto dll = mat::GetExecutableDirectory() / L"ScreenTabHook.dll";
    const auto injection = mat::InjectLibrary(pid, dll, error);
    if (injection != mat::InjectionResult::Failed) {
        g_pendingPid = pid;
        g_pendingSince = GetTickCount64();
        g_pendingWarned = false;
        g_pendingTimedOut = false;
        SetTrayStatus(hwnd, injection == mat::InjectionResult::Pending
                                ? L"Waiting for Explorer to load hook"
                                : L"Starting hook");
    } else {
        InterlockedExchange(&g_state->symbolsReady, 0);
        g_nextResolveTick = GetTickCount64() + kResolveRetryMs;
        ReportProblem(hwnd, error, L"Hook unavailable");
    }
}

void ShowMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    const std::wstring status = L"Status: " +
        (g_paused ? std::wstring(L"Paused") : g_status);
    AppendMenuW(menu, MF_STRING, ID_STATUS, status.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (g_paused ? MF_CHECKED : 0),
                ID_PAUSE, L"Pause");
    AppendMenuW(menu, MF_STRING |
                (IsStartWithWindowsEnabled() ? MF_CHECKED : 0),
                ID_STARTUP, L"Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_ABOUT, L"About");
    AppendMenuW(menu, MF_STRING, ID_EXIT, L"Exit");
    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_taskbarCreatedMessage && msg == g_taskbarCreatedMessage) {
        AddTrayIcon(hwnd);
        EnsureInjected(hwnd);
        return 0;
    }
    switch (msg) {
    case WM_TRAY:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU)
            ShowMenu(hwnd);
        return 0;
    case WM_RESOLVED: {
        std::unique_ptr<ResolveResult> result(
            reinterpret_cast<ResolveResult*>(lParam));
        g_resolving = false;
        if (result->explorerPid != g_explorerPid) {
            EnsureInjected(hwnd);
            return 0;
        }
        if (result->success) {
            g_state->symbols = result->symbols;
            g_state->moduleIdentity = result->identity;
            InterlockedExchange(&g_state->symbolsReady, 1);
            g_nextResolveTick = 0;
            EnsureInjected(hwnd);
        } else {
            g_nextResolveTick = GetTickCount64() + kResolveRetryMs;
            ReportProblem(hwnd, result->error, L"Waiting to retry");
        }
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_STATUS:
            MessageBoxW(hwnd,
                        g_lastProblem.empty() ? g_status.c_str()
                                              : g_lastProblem.c_str(),
                        L"ScreenTab status", MB_OK | MB_ICONINFORMATION);
            break;
        case ID_PAUSE:
            g_paused = !g_paused;
            InterlockedExchange(&g_state->enabled,
                                !g_paused && g_activePid != 0 &&
                                        g_activePid == g_explorerPid ? 1 : 0);
            SetTrayStatus(hwnd, g_status);
            break;
        case ID_STARTUP:
            SetStartWithWindows(!IsStartWithWindowsEnabled());
            break;
        case ID_ABOUT:
            MessageBoxW(hwnd,
                        L"ScreenTab 0.1.1\nNative Alt+Tab, filtered to the active monitor.",
                        L"About ScreenTab", MB_OK | MB_ICONINFORMATION);
            break;
        case ID_EXIT:
            InterlockedExchange(&g_state->enabled, 0);
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    case WM_TIMER:
        EnsureInjected(hwnd);
        return 0;
    case WM_CLOSE:
        if (g_state) InterlockedExchange(&g_state->enabled, 0);
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY: {
        NOTIFYICONDATAW nid{sizeof(nid)};
        nid.hWnd = hwnd;
        nid.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool RequestExistingInstanceShutdown() {
    HWND hwnd = FindWindowW(L"ScreenTab.Controller", L"ScreenTab");
    if (!hwnd) return true;
    DWORD_PTR result = 0;
    return SendMessageTimeoutW(hwnd, WM_CLOSE, 0, 0,
                               SMTO_ABORTIFHUNG | SMTO_BLOCK, 5000,
                               &result) != 0;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    if (wcsstr(GetCommandLineW(), L"--shutdown")) {
        return RequestExistingInstanceShutdown() ? 0 : 1;
    }
    if (wcsstr(GetCommandLineW(), L"--install-startup")) {
        SetStartWithWindows(true);
        return 0;
    }

    HANDLE mutex = CreateMutexW(nullptr, TRUE,
                                L"Local\\ScreenTab.Controller.v1");
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    g_mapping = CreateFileMappingW(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
        sizeof(mat::SharedState), mat::kSharedMappingName);
    if (!g_mapping) return 2;
    g_state = static_cast<mat::SharedState*>(MapViewOfFile(
        g_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(mat::SharedState)));
    if (!g_state) return 3;
    ZeroMemory(g_state, sizeof(*g_state));
    g_state->protocolVersion = mat::kProtocolVersion;
    InterlockedExchange(&g_state->enabled, 0);

    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"ScreenTab.Controller";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"ScreenTab",
                                WS_OVERLAPPED, 0, 0, 0, 0,
                                nullptr, nullptr, instance, nullptr);

    AddTrayIcon(hwnd);

    EnsureInjected(hwnd);
    SetTimer(hwnd, ID_TIMER, 2000, nullptr);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    InterlockedExchange(&g_state->enabled, 0);
    if (g_activePid || g_pendingPid) {
        const DWORD pid = g_activePid ? g_activePid : g_pendingPid;
        if (g_pendingPid) {
            const ULONGLONG deadline = GetTickCount64() + 5000;
            while (GetTickCount64() < deadline &&
                   !HookInitThreadFinished(pid)) {
                Sleep(25);
            }
        }
        const LONG status = InterlockedCompareExchange(&g_state->hookStatus, 0, 0);
        if (g_activePid || (g_pendingPid &&
            status == static_cast<LONG>(mat::HookStatus::Ready) &&
            HookInitThreadFinished(pid))) {
            std::wstring unloadError;
            mat::UninjectLibrary(pid, L"ScreenTabHook.dll", unloadError);
        }
    }
    UnmapViewOfFile(g_state);
    CloseHandle(g_mapping);
    CloseHandle(mutex);
    return 0;
}
