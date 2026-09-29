#include "controller/controller.h"
#include "common/protocol.h"
#include "controller/resource.h"
#include <shellapi.h>
#include <shlwapi.h>
#include <filesystem>
#include <string>

namespace {
constexpr UINT WM_TRAY = WM_APP + 10;
constexpr UINT ID_PAUSE = 1001;
constexpr UINT ID_STARTUP = 1002;
constexpr UINT ID_ABOUT = 1003;
constexpr UINT ID_EXIT = 1004;
constexpr UINT_PTR ID_TIMER = 42;

HANDLE g_mapping = nullptr;
mat::SharedState* g_state = nullptr;
DWORD g_injectedPid = 0;
bool g_paused = false;

bool IsStartWithWindowsEnabled() {
    HKEY key{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
    wchar_t value[32768]{}; DWORD type=0, size=sizeof(value);
    bool enabled = RegQueryValueExW(key, L"ScreenTab", nullptr, &type, reinterpret_cast<BYTE*>(value), &size) == ERROR_SUCCESS;
    RegCloseKey(key);
    return enabled;
}

void SetStartWithWindows(bool enabled) {
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    if (enabled) {
        wchar_t exe[32768]{}; GetModuleFileNameW(nullptr, exe, 32768);
        std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --startup";
        RegSetValueExW(key, L"ScreenTab", 0, REG_SZ, reinterpret_cast<const BYTE*>(cmd.c_str()), static_cast<DWORD>((cmd.size()+1)*sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, L"ScreenTab");
    }
    RegCloseKey(key);
}

void Balloon(HWND hwnd, const wchar_t* title, const std::wstring& message) {
    NOTIFYICONDATAW nid{sizeof(nid)}; nid.hWnd=hwnd; nid.uID=1; nid.uFlags=NIF_INFO;
    wcsncpy_s(nid.szInfoTitle, title, _TRUNCATE); wcsncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
    nid.dwInfoFlags=NIIF_WARNING; Shell_NotifyIconW(NIM_MODIFY,&nid);
}

void EnsureInjected(HWND hwnd) {
    if (!g_state || InterlockedCompareExchange(&g_state->symbolsReady,0,0)==0) return;
    DWORD pid = mat::FindExplorerProcessId();
    if (!pid || pid == g_injectedPid) return;
    g_state->explorerPid = pid;
    auto dll = mat::GetExecutableDirectory() / L"ScreenTabHook.dll";
    std::wstring error;
    if (mat::InjectLibrary(pid, dll, error)) g_injectedPid = pid;
    else Balloon(hwnd, L"ScreenTab", error);
}

void ShowMenu(HWND hwnd) {
    HMENU menu=CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (g_paused?MF_CHECKED:0), ID_PAUSE, L"Pause");
    AppendMenuW(menu, MF_STRING | (IsStartWithWindowsEnabled()?MF_CHECKED:0), ID_STARTUP, L"Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu, MF_STRING, ID_ABOUT, L"About");
    AppendMenuW(menu, MF_STRING, ID_EXIT, L"Exit");
    POINT pt{}; GetCursorPos(&pt); SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
    case WM_TRAY:
        if (lParam==WM_RBUTTONUP || lParam==WM_CONTEXTMENU) ShowMenu(hwnd);
        return 0;
    case WM_COMMAND:
        switch(LOWORD(wParam)) {
        case ID_PAUSE: g_paused=!g_paused; InterlockedExchange(&g_state->enabled, g_paused?0:1); break;
        case ID_STARTUP: SetStartWithWindows(!IsStartWithWindowsEnabled()); break;
        case ID_ABOUT: MessageBoxW(hwnd,L"ScreenTab 0.1\nNative Alt+Tab, filtered to the active monitor.",L"About ScreenTab",MB_OK|MB_ICONINFORMATION); break;
        case ID_EXIT: InterlockedExchange(&g_state->enabled,0); DestroyWindow(hwnd); break;
        }
        return 0;
    case WM_TIMER: EnsureInjected(hwnd); return 0;
    case WM_DESTROY: {
        NOTIFYICONDATAW nid{sizeof(nid)}; nid.hWnd=hwnd; nid.uID=1; Shell_NotifyIconW(NIM_DELETE,&nid);
        PostQuitMessage(0); return 0;
    }
    }
    return DefWindowProcW(hwnd,msg,wParam,lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    if (wcsstr(GetCommandLineW(), L"--install-startup")) SetStartWithWindows(true);
    HANDLE mutex=CreateMutexW(nullptr,TRUE,L"Local\\ScreenTab.Controller.v1");
    if (!mutex || GetLastError()==ERROR_ALREADY_EXISTS) return 0;

    g_mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(mat::SharedState),mat::kSharedMappingName);
    if (!g_mapping) return 2;
    g_state=static_cast<mat::SharedState*>(MapViewOfFile(g_mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(mat::SharedState)));
    if (!g_state) return 3;
    ZeroMemory(g_state,sizeof(*g_state));
    g_state->protocolVersion=mat::kProtocolVersion;
    InterlockedExchange(&g_state->enabled,1);

    std::wstring symbolError;
    if (mat::ResolveTwinuiSymbols(g_state->symbols,symbolError)) InterlockedExchange(&g_state->symbolsReady,1);

    WNDCLASSW wc{}; wc.lpfnWndProc=WndProc; wc.hInstance=instance; wc.lpszClassName=L"ScreenTab.Controller";
    RegisterClassW(&wc);
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"ScreenTab",WS_OVERLAPPED,0,0,0,0,nullptr,nullptr,instance,nullptr);

    NOTIFYICONDATAW nid{sizeof(nid)}; nid.hWnd=hwnd; nid.uID=1; nid.uFlags=NIF_MESSAGE|NIF_TIP|NIF_ICON; nid.uCallbackMessage=WM_TRAY;
    nid.hIcon=LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDI_APPICON)); wcscpy_s(nid.szTip,L"ScreenTab"); Shell_NotifyIconW(NIM_ADD,&nid);

    if (!g_state->symbolsReady) Balloon(hwnd,L"ScreenTab",symbolError + L". Standard Alt+Tab is unchanged.");
    else EnsureInjected(hwnd);
    SetTimer(hwnd,ID_TIMER,2000,nullptr);

    MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}    
    if(g_state) UnmapViewOfFile(g_state); if(g_mapping) CloseHandle(g_mapping); if(mutex) CloseHandle(mutex);
    return 0;
}
