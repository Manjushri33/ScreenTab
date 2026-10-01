#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include "common/protocol.h"

namespace mat {
enum class InjectionResult {
    Loaded,
    Pending,
    Failed,
};

bool ResolveTwinuiSymbols(SymbolOffsets& out, ModuleIdentity& identity,
                          std::wstring& error);
DWORD FindExplorerProcessId();
InjectionResult InjectLibrary(DWORD pid, const std::filesystem::path& dllPath,
                              std::wstring& error);
bool UninjectLibrary(DWORD pid, const std::wstring& moduleName, std::wstring& error);
std::filesystem::path GetExecutableDirectory();
}
