#include "controller/controller.h"
#include <dbghelp.h>
#include <windows.h>
#include <array>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>

#pragma comment(lib, "dbghelp.lib")

namespace mat {
namespace {
struct Wanted {
    const wchar_t* label;
    const wchar_t* needle1;
    const wchar_t* needle2;
    std::uint64_t SymbolOffsets::* member;
};

// DbgHelp's undecorated names can vary slightly between Windows/PDB versions.
// Match stable class/function identifiers instead of complete demangled signatures.
constexpr Wanted kWanted[] = {
    {L"CVirtualDesktop::IsViewVisible", L"CVirtualDesktop::IsViewVisible", nullptr,
     &SymbolOffsets::virtualDesktopIsViewVisible},
    {L"CWin32ApplicationView IApplicationView vtable",
     L"CWin32ApplicationView::`vftable'", L"IApplicationView",
     &SymbolOffsets::win32ViewVtable},
    {L"CWin32ApplicationView::v_GetNativeWindow",
     L"CWin32ApplicationView::v_GetNativeWindow", nullptr,
     &SymbolOffsets::win32GetNativeWindow},
    {L"CWinRTApplicationView IApplicationView vtable",
     L"CWinRTApplicationView::`vftable'", L"IApplicationView",
     &SymbolOffsets::winrtViewVtable},
    {L"CWinRTApplicationView::v_GetNativeWindow",
     L"CWinRTApplicationView::v_GetNativeWindow", nullptr,
     &SymbolOffsets::winrtGetNativeWindow},
    {L"XamlAltTabViewHost::Show", L"XamlAltTabViewHost::Show", nullptr,
     &SymbolOffsets::xamlAltTabShow},
    {L"ITaskGroupWindowInformation::Position", L"ITaskGroupWindowInformation", L"::Position",
     &SymbolOffsets::taskGroupPosition},
    {L"XamlAltTabViewHost_CreateInstance", L"XamlAltTabViewHost_CreateInstance", nullptr,
     &SymbolOffsets::xamlAltTabCreateInstance},
};

struct EnumContext {
    DWORD64 base{};
    SymbolOffsets* offsets{};
};

bool Matches(std::wstring_view name, const Wanted& wanted) {
    if (name.find(wanted.needle1) == std::wstring_view::npos) return false;
    return !wanted.needle2 || name.find(wanted.needle2) != std::wstring_view::npos;
}

BOOL CALLBACK EnumSymbols(PSYMBOL_INFOW info, ULONG, PVOID user) {
    auto* ctx = static_cast<EnumContext*>(user);
    std::wstring_view name(info->Name, info->NameLen);
    for (const auto& wanted : kWanted) {
        auto& slot = ctx->offsets->*(wanted.member);
        if (slot != 0) continue;
        if (Matches(name, wanted)) {
            slot = info->Address - ctx->base;
        }
    }
    return TRUE;
}

bool Complete(const SymbolOffsets& s) {
    return s.virtualDesktopIsViewVisible && s.win32ViewVtable &&
           s.win32GetNativeWindow && s.winrtViewVtable &&
           s.winrtGetNativeWindow && s.xamlAltTabShow &&
           s.taskGroupPosition && s.xamlAltTabCreateInstance;
}

std::wstring MissingSymbols(const SymbolOffsets& offsets) {
    std::wostringstream stream;
    bool first = true;
    for (const auto& wanted : kWanted) {
        if (offsets.*(wanted.member) != 0) continue;
        if (!first) stream << L", ";
        stream << wanted.label;
        first = false;
    }
    return stream.str();
}
}

bool ResolveTwinuiSymbols(SymbolOffsets& out, std::wstring& error) {
    out = {};

    wchar_t systemDir[MAX_PATH]{};
    if (!GetSystemDirectoryW(systemDir, MAX_PATH)) {
        error = L"GetSystemDirectory failed";
        return false;
    }
    std::filesystem::path modulePath = std::filesystem::path(systemDir) / L"twinui.pcshell.dll";

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS);

    wchar_t localAppData[MAX_PATH]{};
    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%", localAppData, MAX_PATH);
    auto cache = std::filesystem::path(localAppData) / L"ScreenTab" / L"symbols";
    std::error_code ec;
    std::filesystem::create_directories(cache, ec);
    std::wstring symbolPath = L"srv*" + cache.wstring() + L"*https://msdl.microsoft.com/download/symbols";

    if (!SymInitializeW(process, symbolPath.c_str(), FALSE)) {
        error = L"Could not initialize Microsoft symbol resolver";
        return false;
    }

    DWORD64 base = SymLoadModuleExW(process, nullptr, modulePath.c_str(), nullptr, 0, 0, nullptr, 0);
    if (!base) {
        error = L"Microsoft symbols for twinui.pcshell.dll are unavailable for this Windows build";
        SymCleanup(process);
        return false;
    }

    EnumContext ctx{base, &out};
    const BOOL enumerated = SymEnumSymbolsW(process, base, nullptr, EnumSymbols, &ctx);
    if (!enumerated || !Complete(out)) {
        error = L"Required Alt+Tab symbols were not found for this Windows build";
        const auto missing = MissingSymbols(out);
        if (!missing.empty()) error += L". Missing symbols: " + missing;
        SymUnloadModule64(process, base);
        SymCleanup(process);
        return false;
    }

    SymUnloadModule64(process, base);
    SymCleanup(process);
    return true;
}
}
