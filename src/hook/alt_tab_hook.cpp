#include "hook/alt_tab_hook.h"
#include "common/protocol.h"
#include <windows.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <MinHook.h>

namespace {
constexpr ULONGLONG kDeltaThresholdMs = 250;
mat::SharedState* g_state=nullptr;
HANDLE g_mapping=nullptr;
HMODULE g_twinui=nullptr;
std::atomic<DWORD> g_createThread{};
std::atomic<DWORD> g_lastCreateThread{};
std::atomic<ULONGLONG> g_createTick{};
std::atomic<DWORD> g_showThread{};
std::atomic<HMONITOR> g_targetMonitor{};

using IsViewVisibleFn = HRESULT(WINAPI*)(void*,void*,BOOL*);
using GetNativeWindowFn = HRESULT(WINAPI*)(void*,HWND*);
using ShowFn = HRESULT(WINAPI*)(void*,void*,int,void*);
using CreateFn = HRESULT(WINAPI*)(void*,void*,void*,void*);
struct RectF { float x,y,width,height; };
using PositionFn = HRESULT(WINAPI*)(void*,RectF*);

IsViewVisibleFn g_isViewVisibleOriginal=nullptr;
GetNativeWindowFn g_win32GetNativeWindow=nullptr;
GetNativeWindowFn g_winrtGetNativeWindow=nullptr;
void* g_win32Vtable=nullptr;
void* g_winrtVtable=nullptr;
ShowFn g_showOriginal=nullptr;
CreateFn g_createOriginal=nullptr;
PositionFn g_positionOriginal=nullptr;

bool Enabled(){return g_state && g_state->protocolVersion==mat::kProtocolVersion && InterlockedCompareExchange(&g_state->enabled,0,0)!=0;}

HMONITOR CurrentWorkMonitor() {
    HWND foreground=GetForegroundWindow();
    if (foreground) {
        HMONITOR mon=MonitorFromWindow(foreground,MONITOR_DEFAULTTONEAREST);
        if(mon) return mon;
    }
    POINT pt{}; if(GetCursorPos(&pt)) return MonitorFromPoint(pt,MONITOR_DEFAULTTONEAREST);
    return MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY);
}

bool IsAltTabFilterWindowActive() {
    if(!Enabled()) return false;
    DWORD tid=GetCurrentThreadId();
    if(g_createThread.load()==tid) return true;
    return g_lastCreateThread.load()==tid && (GetTickCount64()-g_createTick.load())<=kDeltaThresholdMs;
}

HRESULT GetWindowHandleFromApplicationView(void* view, HWND* hwnd) {
    *hwnd=nullptr; if(!view) return E_INVALIDARG;
    void* vtable=*reinterpret_cast<void**>(view);
    if(vtable==g_win32Vtable) return g_win32GetNativeWindow(view,hwnd);
    if(vtable==g_winrtVtable) return g_winrtGetNativeWindow(view,hwnd);
    return E_NOINTERFACE;
}

HRESULT WINAPI CVirtualDesktop_IsViewVisible_Hook(void* self, void* view, BOOL* visible) {
    HRESULT originalResult=g_isViewVisibleOriginal(self,view,visible);
    if(FAILED(originalResult) || !visible || !*visible || !IsAltTabFilterWindowActive()) return originalResult;
    HWND hwnd{}; if(FAILED(GetWindowHandleFromApplicationView(view,&hwnd)) || !hwnd) return originalResult;
    HMONITOR target=g_targetMonitor.load(); if(!target) target=CurrentWorkMonitor();
    if(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST)!=target) *visible=FALSE;
    return originalResult;
}

HRESULT WINAPI XamlAltTabViewHost_Show_Hook(void* self, void* p1, int p2, void* p3) {
    g_showThread=GetCurrentThreadId();
    HRESULT hr=g_showOriginal(self,p1,p2,p3);
    g_showThread=0; return hr;
}

HRESULT WINAPI TaskGroupPosition_Hook(void* self, RectF* rect) {
    if(Enabled() && g_showThread.load()==GetCurrentThreadId()) {
        g_showThread=0;
        HMONITOR mon=g_targetMonitor.load(); if(!mon) mon=CurrentWorkMonitor();
        MONITORINFO mi{sizeof(mi)};
        if(GetMonitorInfoW(mon,&mi)) {
            RectF desired{static_cast<float>(mi.rcWork.left),static_cast<float>(mi.rcWork.top),
                          static_cast<float>(mi.rcWork.right-mi.rcWork.left),static_cast<float>(mi.rcWork.bottom-mi.rcWork.top)};
            return g_positionOriginal(self,&desired);
        }
    }
    return g_positionOriginal(self,rect);
}

HRESULT WINAPI XamlAltTabViewHost_CreateInstance_Hook(void* self, void* p1, void* p2, void* p3) {
    if(!Enabled()) return g_createOriginal(self,p1,p2,p3);
    DWORD tid=GetCurrentThreadId();
    g_targetMonitor=CurrentWorkMonitor();
    g_createThread=tid; g_lastCreateThread=tid; g_createTick=GetTickCount64();
    HRESULT hr=g_createOriginal(self,p1,p2,p3);
    g_createThread=0; return hr;
}

bool RvaIsInImage(std::uint64_t rva) {
    if (!g_twinui || !rva) return false;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(g_twinui);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(reinterpret_cast<std::byte*>(g_twinui) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    return rva < nt->OptionalHeader.SizeOfImage;
}

template<class T>
bool HookAt(std::uint64_t rva, void* hook, T* original) {
    if(!RvaIsInImage(rva)) return false;
    void* target=reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(g_twinui)+rva);
    return MH_CreateHook(target,hook,reinterpret_cast<void**>(original))==MH_OK;
}
}

bool InitializeAltTabHooks() {
    g_mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,mat::kSharedMappingName);
    if(!g_mapping) return false;
    g_state=static_cast<mat::SharedState*>(MapViewOfFile(g_mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(mat::SharedState)));
    if(!g_state || !g_state->symbolsReady) return false;
    g_twinui=GetModuleHandleW(L"twinui.pcshell.dll");
    if(!g_twinui) g_twinui=LoadLibraryExW(L"twinui.pcshell.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!g_twinui || MH_Initialize()!=MH_OK) return false;

    auto base=reinterpret_cast<std::uintptr_t>(g_twinui);
    const auto& s=g_state->symbols;
    if(!RvaIsInImage(s.win32ViewVtable) || !RvaIsInImage(s.win32GetNativeWindow) ||
       !RvaIsInImage(s.winrtViewVtable) || !RvaIsInImage(s.winrtGetNativeWindow)) return false;
    g_win32Vtable=reinterpret_cast<void*>(base+s.win32ViewVtable);
    g_winrtVtable=reinterpret_cast<void*>(base+s.winrtViewVtable);
    g_win32GetNativeWindow=reinterpret_cast<GetNativeWindowFn>(base+s.win32GetNativeWindow);
    g_winrtGetNativeWindow=reinterpret_cast<GetNativeWindowFn>(base+s.winrtGetNativeWindow);

    bool ok=true;
    ok &= HookAt(s.virtualDesktopIsViewVisible,reinterpret_cast<void*>(CVirtualDesktop_IsViewVisible_Hook),&g_isViewVisibleOriginal);
    ok &= HookAt(s.xamlAltTabShow,reinterpret_cast<void*>(XamlAltTabViewHost_Show_Hook),&g_showOriginal);
    ok &= HookAt(s.taskGroupPosition,reinterpret_cast<void*>(TaskGroupPosition_Hook),&g_positionOriginal);
    ok &= HookAt(s.xamlAltTabCreateInstance,reinterpret_cast<void*>(XamlAltTabViewHost_CreateInstance_Hook),&g_createOriginal);
    if(!ok || MH_EnableHook(MH_ALL_HOOKS)!=MH_OK) { MH_Uninitialize(); return false; }
    return true;
}

void ShutdownAltTabHooks() {
    if(g_state) InterlockedExchange(&g_state->enabled,0);
    MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize();
    if(g_state){UnmapViewOfFile(g_state);g_state=nullptr;}
    if(g_mapping){CloseHandle(g_mapping);g_mapping=nullptr;}
}
