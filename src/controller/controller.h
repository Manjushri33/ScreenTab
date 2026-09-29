#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include "common/protocol.h"

namespace mat {
bool ResolveTwinuiSymbols(SymbolOffsets& out, std::wstring& error);
DWORD FindExplorerProcessId();
bool InjectLibrary(DWORD pid, const std::filesystem::path& dllPath, std::wstring& error);
std::filesystem::path GetExecutableDirectory();
}
