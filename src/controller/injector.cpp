#include "controller/controller.h"
#include <tlhelp32.h>
#include <vector>

namespace mat {
DWORD FindExplorerProcessId() {
    HWND shell = GetShellWindow();
    DWORD pid = 0;
    if (shell) GetWindowThreadProcessId(shell, &pid);
    return pid;
}

bool InjectLibrary(DWORD pid, const std::filesystem::path& dllPath, std::wstring& error) {
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                 PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process) {
        error = L"Could not open Explorer for injection";
        return false;
    }

    std::wstring full = std::filesystem::absolute(dllPath).wstring();
    SIZE_T bytes = (full.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        error = L"Could not allocate memory in Explorer";
        CloseHandle(process);
        return false;
    }

    bool ok = WriteProcessMemory(process, remote, full.c_str(), bytes, nullptr) != FALSE;
    auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    HANDLE thread = ok ? CreateRemoteThread(process, nullptr, 0, loadLibrary, remote, 0, nullptr) : nullptr;
    if (!thread) ok = false;
    if (thread) {
        WaitForSingleObject(thread, 10000);
        DWORD exitCode = 0;
        GetExitCodeThread(thread, &exitCode);
        ok = ok && exitCode != 0;
        CloseHandle(thread);
    }
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);

    if (!ok) error = L"Explorer did not load ScreenTabHook.dll";
    return ok;
}

namespace {
bool FindRemoteModule(DWORD pid, const std::wstring& moduleName, MODULEENTRY32W& out) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    MODULEENTRY32W entry{sizeof(entry)};
    bool found = false;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, moduleName.c_str()) == 0) {
                out = entry;
                found = true;
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}
}

bool UninjectLibrary(DWORD pid, const std::wstring& moduleName, std::wstring& error) {
    if (!pid) return true;

    MODULEENTRY32W remoteModule{sizeof(remoteModule)};
    if (!FindRemoteModule(pid, moduleName, remoteModule)) {
        return true;
    }

    MODULEENTRY32W remoteKernel32{sizeof(remoteKernel32)};
    if (!FindRemoteModule(pid, L"kernel32.dll", remoteKernel32)) {
        error = L"Could not locate kernel32.dll in Explorer";
        return false;
    }

    HMODULE localKernel32 = GetModuleHandleW(L"kernel32.dll");
    auto localFreeLibrary = reinterpret_cast<std::uintptr_t>(
        GetProcAddress(localKernel32, "FreeLibrary"));
    if (!localKernel32 || !localFreeLibrary) {
        error = L"Could not locate FreeLibrary";
        return false;
    }

    const auto freeLibraryRva = localFreeLibrary - reinterpret_cast<std::uintptr_t>(localKernel32);
    auto remoteFreeLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        reinterpret_cast<std::uintptr_t>(remoteKernel32.modBaseAddr) + freeLibraryRva);

    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                 PROCESS_VM_OPERATION | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process) {
        error = L"Could not open Explorer for hook unload";
        return false;
    }

    HANDLE thread = CreateRemoteThread(process, nullptr, 0, remoteFreeLibrary,
                                       remoteModule.modBaseAddr, 0, nullptr);
    if (!thread) {
        CloseHandle(process);
        error = L"Could not unload ScreenTabHook.dll from Explorer";
        return false;
    }

    const DWORD wait = WaitForSingleObject(thread, 10000);
    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);
    CloseHandle(process);

    if (wait != WAIT_OBJECT_0 || exitCode == 0) {
        error = L"Explorer did not unload ScreenTabHook.dll";
        return false;
    }
    return true;
}

std::filesystem::path GetExecutableDirectory() {
    std::vector<wchar_t> buffer(32768);
    DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring(buffer.data(), len)).parent_path();
}
}
