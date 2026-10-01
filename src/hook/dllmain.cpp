#include <windows.h>
#include "hook/alt_tab_hook.h"

namespace {
void ReportHookStatus(mat::HookStatus status) {
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE,
                                      mat::kSharedMappingName);
    if (!mapping) return;
    auto* state = static_cast<mat::SharedState*>(MapViewOfFile(
        mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(mat::SharedState)));
    if (state) {
        if (state->protocolVersion == mat::kProtocolVersion &&
            state->explorerPid == GetCurrentProcessId()) {
            state->hookInitThreadId = GetCurrentThreadId();
            InterlockedExchange(&state->hookStatus, static_cast<LONG>(status));
        }
        UnmapViewOfFile(state);
    }
    CloseHandle(mapping);
}

DWORD WINAPI InitThread(void* context) {
    const auto status = InitializeAltTabHooks();
    ReportHookStatus(status);
    if (status != mat::HookStatus::Ready)
        FreeLibraryAndExitThread(static_cast<HMODULE>(context), 0);
    return 0;
}
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        HANDLE thread=CreateThread(nullptr,0,InitThread,instance,0,nullptr);
        if(thread) CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH) {
        ShutdownAltTabHooks();
    }
    return TRUE;
}
