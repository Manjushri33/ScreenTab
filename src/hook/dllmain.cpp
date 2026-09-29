#include <windows.h>
#include "hook/alt_tab_hook.h"

DWORD WINAPI InitThread(void*) { InitializeAltTabHooks(); return 0; }

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        HANDLE thread=CreateThread(nullptr,0,InitThread,nullptr,0,nullptr);
        if(thread) CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH) {
        ShutdownAltTabHooks();
    }
    return TRUE;
}
