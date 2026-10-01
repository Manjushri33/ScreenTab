#include "controller/controller.h"
#include <tlhelp32.h>
#include <vector>

namespace mat {
namespace {
enum class ModuleLookup { Found, Missing, Error };

ModuleLookup FindRemoteModule(DWORD pid, const std::wstring& moduleName,
                              MODULEENTRY32W& out) {
    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return ModuleLookup::Error;

    MODULEENTRY32W entry{sizeof(entry)};
    if (!Module32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        return ModuleLookup::Error;
    }
    do {
        if (_wcsicmp(entry.szModule, moduleName.c_str()) == 0) {
            out = entry;
            CloseHandle(snapshot);
            return ModuleLookup::Found;
        }
    } while (Module32NextW(snapshot, &entry));
    const DWORD scanError = GetLastError();
    CloseHandle(snapshot);
    return scanError == ERROR_NO_MORE_FILES ? ModuleLookup::Missing
                                             : ModuleLookup::Error;
}
}

DWORD FindExplorerProcessId() {
    HWND shell = GetShellWindow();
    DWORD pid = 0;
    if (shell) GetWindowThreadProcessId(shell, &pid);
    return pid;
}

InjectionResult InjectLibrary(DWORD pid, const std::filesystem::path& dllPath,
                              std::wstring& error) {
    MODULEENTRY32W existing{sizeof(existing)};
    const auto lookup = FindRemoteModule(pid, L"ScreenTabHook.dll", existing);
    if (lookup != ModuleLookup::Missing) {
        error = lookup == ModuleLookup::Found
            ? L"Explorer already contains ScreenTabHook.dll. Restart Explorer or Windows"
            : L"Could not inspect Explorer modules before injection";
        return InjectionResult::Failed;
    }
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                 PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process) {
        error = L"Could not open Explorer for injection";
        return InjectionResult::Failed;
    }

    std::wstring full = std::filesystem::absolute(dllPath).wstring();
    SIZE_T bytes = (full.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        error = L"Could not allocate memory in Explorer";
        CloseHandle(process);
        return InjectionResult::Failed;
    }

    bool ok = WriteProcessMemory(process, remote, full.c_str(), bytes, nullptr) != FALSE;
    auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    HANDLE thread = ok ? CreateRemoteThread(process, nullptr, 0, loadLibrary, remote, 0, nullptr) : nullptr;
    if (!thread) ok = false;
    DWORD wait = WAIT_FAILED;
    if (thread) {
        wait = WaitForSingleObject(thread, 10000);
        DWORD exitCode = 0;
        ok = ok && wait == WAIT_OBJECT_0 &&
             GetExitCodeThread(thread, &exitCode) && exitCode != 0;
        CloseHandle(thread);
    }
    if (!thread) VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    // The remote thread may still be reading the path when the wait times out.
    if (wait == WAIT_OBJECT_0) VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);

    if (thread && wait != WAIT_OBJECT_0) {
        error = L"Explorer is still loading ScreenTabHook.dll";
        return InjectionResult::Pending;
    }
    if (!ok) {
        error = L"Explorer did not load ScreenTabHook.dll";
        return InjectionResult::Failed;
    }
    return InjectionResult::Loaded;
}

bool UninjectLibrary(DWORD pid, const std::wstring& moduleName, std::wstring& error) {
    if (!pid) return true;

    MODULEENTRY32W remoteModule{sizeof(remoteModule)};
    const auto moduleLookup = FindRemoteModule(pid, moduleName, remoteModule);
    if (moduleLookup == ModuleLookup::Missing) return true;
    if (moduleLookup == ModuleLookup::Error) {
        error = L"Could not inspect Explorer modules for hook unload";
        return false;
    }

    MODULEENTRY32W remoteKernel32{sizeof(remoteKernel32)};
    if (FindRemoteModule(pid, L"kernel32.dll", remoteKernel32) !=
        ModuleLookup::Found) {
        error = L"Could not locate kernel32.dll in Explorer";
        return false;
    }

    HMODULE localKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!localKernel32) {
        error = L"Could not locate kernel32.dll locally";
        return false;
    }
    auto localFreeLibrary = reinterpret_cast<std::uintptr_t>(
        GetProcAddress(localKernel32, "FreeLibrary"));
    if (!localFreeLibrary) {
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
    MODULEENTRY32W remaining{sizeof(remaining)};
    const auto after = FindRemoteModule(pid, moduleName, remaining);
    if (after != ModuleLookup::Missing) {
        error = after == ModuleLookup::Found
            ? L"ScreenTabHook.dll remains loaded in Explorer"
            : L"Could not verify ScreenTabHook.dll was unloaded";
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
