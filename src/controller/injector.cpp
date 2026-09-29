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

std::filesystem::path GetExecutableDirectory() {
    std::vector<wchar_t> buffer(32768);
    DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring(buffer.data(), len)).parent_path();
}
}
