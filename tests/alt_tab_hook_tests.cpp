#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
DWORD testThread = 10;
ULONGLONG testTick = 10000;
HWND foreground = reinterpret_cast<HWND>(1);
HMONITOR primary = reinterpret_cast<HMONITOR>(1);
HMONITOR secondary = reinterpret_cast<HMONITOR>(2);
HMONITOR workMonitor = secondary;
HMONITOR cursorMonitor = secondary;
HWND desktop = reinterpret_cast<HWND>(9);
bool foregroundVisible = true;
std::wstring foregroundClass = L"Chrome_WidgetWin_1";
DWORD WINAPI TestGetCurrentThreadId() { return testThread; }
ULONGLONG WINAPI TestGetTickCount64() { return testTick; }
HWND WINAPI TestGetForegroundWindow() { return foreground; }
HMONITOR WINAPI TestMonitorFromWindow(HWND window, DWORD) {
    return window == foreground ? workMonitor : primary;
}
BOOL WINAPI TestIsWindowVisible(HWND) { return foregroundVisible; }
HWND WINAPI TestGetShellWindow() { return desktop; }
int WINAPI TestGetClassNameW(HWND, LPWSTR buffer, int size) {
    wcsncpy_s(buffer, size, foregroundClass.c_str(), _TRUNCATE);
    return static_cast<int>(wcslen(buffer));
}
BOOL WINAPI TestGetCursorPos(LPPOINT point) { *point = {100,100}; return TRUE; }
HMONITOR WINAPI TestMonitorFromPoint(POINT, DWORD) { return cursorMonitor; }
}

// Only the external OS inputs are controlled; exercise the production hooks.
#define GetCurrentThreadId TestGetCurrentThreadId
#define GetTickCount64 TestGetTickCount64
#define GetForegroundWindow TestGetForegroundWindow
#define MonitorFromWindow TestMonitorFromWindow
#define IsWindowVisible TestIsWindowVisible
#define GetShellWindow TestGetShellWindow
#define GetClassNameW TestGetClassNameW
#define GetCursorPos TestGetCursorPos
#define MonitorFromPoint TestMonitorFromPoint
#include "hook/alt_tab_hook.cpp"
#undef GetCurrentThreadId
#undef GetTickCount64
#undef GetForegroundWindow
#undef MonitorFromWindow
#undef IsWindowVisible
#undef GetShellWindow
#undef GetClassNameW
#undef GetCursorPos
#undef MonitorFromPoint

namespace {
mat::SharedState state{};
void* win32Vtable = reinterpret_cast<void*>(100);
void* viewVtable = win32Vtable;
HWND viewWindow = reinterpret_cast<HWND>(3);
bool windowsVisible = true;
BOOL observedVisible = TRUE;
bool positionBeforeFilter = false;
HRESULT WINAPI OriginalVisible(void*, void*, BOOL* visible) {
    *visible = windowsVisible;
    return S_OK;
}
HRESULT WINAPI NativeWindow(void*, HWND* window) {
    *window = viewWindow;
    return S_OK;
}
HRESULT WINAPI OriginalPosition(void*, RectF*) { return S_OK; }
HRESULT WINAPI OriginalCreate(void*, void*, void*, void*) { return S_OK; }
HRESULT WINAPI OriginalShow(void*, void*, int, void*) {
    if (positionBeforeFilter) {
        RectF rect{};
        TaskGroupPosition_Hook(nullptr, &rect);
    }
    observedVisible = TRUE;
    return CVirtualDesktop_IsViewVisible_Hook(nullptr, &viewVtable,
                                             &observedVisible);
}
void Reset() {
    state = {};
    g_state = &state;
    g_createThread = 0;
    g_lastCreateThread = 0;
    g_createTick = 0;
    g_showThread = 0;
    g_targetMonitor = nullptr;
    g_isViewVisibleOriginal = OriginalVisible;
    g_win32Vtable = win32Vtable;
    g_win32GetNativeWindow = NativeWindow;
    g_showOriginal = OriginalShow;
    g_createOriginal = OriginalCreate;
    g_positionOriginal = OriginalPosition;
    testThread = 10;
    testTick = 10000;
    workMonitor = secondary;
    foreground = reinterpret_cast<HWND>(1);
    cursorMonitor = secondary;
    foregroundVisible = true;
    foregroundClass = L"Chrome_WidgetWin_1";
    viewWindow = reinterpret_cast<HWND>(3);
    windowsVisible = true;
    positionBeforeFilter = false;
}
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void ReusedSwitcherFiltersWithoutExplorer() {
    Reset();
    workMonitor = primary;
    XamlAltTabViewHost_CreateInstance_Hook(nullptr, nullptr, nullptr, nullptr);
    testTick += 5000;  // Host survives after the creation grace period.
    workMonitor = secondary;
    XamlAltTabViewHost_Show_Hook(nullptr, nullptr, 0, nullptr);
    Require(!observedVisible, "reused Alt+Tab leaked a window from monitor 1");
    Require(g_targetMonitor.load() == secondary,
            "reused Alt+Tab retained the previous monitor");
}
void PositioningDoesNotStopFiltering() {
    Reset();
    positionBeforeFilter = true;
    XamlAltTabViewHost_Show_Hook(nullptr, nullptr, 0, nullptr);
    Require(!observedVisible, "positioning stopped Alt+Tab filtering");
}
void CurrentMonitorWindowsRemainVisible() {
    Reset();
    viewWindow = foreground;
    XamlAltTabViewHost_Show_Hook(nullptr, nullptr, 0, nullptr);
    Require(observedVisible, "Alt+Tab hid a window from the working monitor");
}
void TaskViewOutsideAltTabRemainsUnfiltered() {
    Reset();
    XamlAltTabViewHost_Show_Hook(nullptr, nullptr, 0, nullptr);
    testTick += 1000;
    BOOL visible = TRUE;
    CVirtualDesktop_IsViewVisible_Hook(nullptr, &viewVtable, &visible);
    Require(visible, "a later Task View query was filtered");
}
void OtherShellThreadsRemainUnfiltered() {
    Reset();
    XamlAltTabViewHost_Show_Hook(nullptr, nullptr, 0, nullptr);
    ++testThread;
    BOOL visible = TRUE;
    CVirtualDesktop_IsViewVisible_Hook(nullptr, &viewVtable, &visible);
    Require(visible, "an unrelated shell thread was filtered");
}
void PausePreservesNormalAltTab() {
    Reset();
    state.enabled = 0;
    XamlAltTabViewHost_Show_Hook(nullptr, nullptr, 0, nullptr);
    Require(observedVisible, "paused ScreenTab filtered a window");
}
void WindowsVisibilityIsPreserved() {
    Reset();
    windowsVisible = false;
    viewWindow = foreground;
    XamlAltTabViewHost_Show_Hook(nullptr, nullptr, 0, nullptr);
    Require(!observedVisible, "ScreenTab restored a window hidden by Windows");
}
void HiddenForegroundFallsBackToCursor() {
    Reset();
    workMonitor = primary;
    foregroundVisible = false;
    Require(CurrentWorkMonitor() == secondary,
            "hidden foreground window selected the primary monitor");
}
void StagingWindowFallsBackToCursor() {
    Reset();
    workMonitor = primary;
    foregroundClass = L"ForegroundStaging";
    XamlAltTabViewHost_CreateInstance_Hook(nullptr, nullptr, nullptr, nullptr);
    Require(g_targetMonitor.load() == secondary,
            "Explorer's staging window overrode the second monitor");
}
void SwitcherHostFallsBackToCursor() {
    for (const auto* shellClass : {L"XamlExplorerHostIslandWindow",
                                  L"XamlExplorerHostIslandWindow_WASDK",
                                  L"MultitaskingViewFrame"}) {
        Reset();
        workMonitor = primary;
        foregroundClass = shellClass;
        XamlAltTabViewHost_Show_Hook(nullptr, nullptr, 0, nullptr);
        Require(!observedVisible,
                "the shell host selected monitor 1 and kept its windows visible");
    }
}
void DesktopFallsBackToCursor() {
    Reset();
    foreground = desktop;
    workMonitor = primary;
    Require(CurrentWorkMonitor() == secondary,
            "the desktop window overrode the cursor monitor");
}
void RealAppKeepsForegroundMonitor() {
    Reset();
    cursorMonitor = primary;
    Require(CurrentWorkMonitor() == secondary,
            "cursor overrode a suitable foreground application's monitor");
}
void ExplorerWindowKeepsForegroundMonitor() {
    Reset();
    foregroundClass = L"CabinetWClass";
    cursorMonitor = primary;
    Require(CurrentWorkMonitor() == secondary,
            "a real File Explorer window was treated as a shell host");
}
}

int wmain() {
    struct Test { const char* name; void (*run)(); };
    const Test tests[] = {
        {"reused switcher filters without Explorer", ReusedSwitcherFiltersWithoutExplorer},
        {"positioning preserves filtering", PositioningDoesNotStopFiltering},
        {"current monitor stays visible", CurrentMonitorWindowsRemainVisible},
        {"Task View stays unfiltered", TaskViewOutsideAltTabRemainsUnfiltered},
        {"other shell threads stay unfiltered", OtherShellThreadsRemainUnfiltered},
        {"pause restores normal behavior", PausePreservesNormalAltTab},
        {"Windows visibility is preserved", WindowsVisibilityIsPreserved},
        {"hidden foreground uses cursor monitor", HiddenForegroundFallsBackToCursor},
        {"foreground staging uses cursor monitor", StagingWindowFallsBackToCursor},
        {"switcher host uses cursor monitor", SwitcherHostFallsBackToCursor},
        {"desktop uses cursor monitor", DesktopFallsBackToCursor},
        {"real app keeps foreground monitor", RealAppKeepsForegroundMonitor},
        {"Explorer window keeps foreground monitor", ExplorerWindowKeepsForegroundMonitor},
    };
    int failures = 0;
    for (const auto& test : tests) {
        try {
            test.run();
            std::cout << "PASS: " << test.name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "FAIL: " << test.name << ": " << error.what() << '\n';
            ++failures;
        }
    }
    return failures ? 1 : 0;
}
