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
    const wchar_t* token1;
    const wchar_t* token2;
    const wchar_t* token3;
    std::uint64_t SymbolOffsets::* member;
};

// DbgHelp can return either undecorated names or raw MSVC decorated names.
// Match stable identifiers which survive both forms instead of relying on one
// exact demangled spelling.
constexpr Wanted kWanted[] = {
    {L"CVirtualDesktop::IsViewVisible", L"CVirtualDesktop", L"IsViewVisible", nullptr,
     &SymbolOffsets::virtualDesktopIsViewVisible},
    {L"CWin32ApplicationView IApplicationView vtable",
     L"CWin32ApplicationView", L"IApplicationView", nullptr,
     &SymbolOffsets::win32ViewVtable},
    {L"CWin32ApplicationView::v_GetNativeWindow",
     L"CWin32ApplicationView", L"v_GetNativeWindow", nullptr,
     &SymbolOffsets::win32GetNativeWindow},
    {L"CWinRTApplicationView IApplicationView vtable",
     L"CWinRTApplicationView", L"IApplicationView", nullptr,
     &SymbolOffsets::winrtViewVtable},
    {L"CWinRTApplicationView::v_GetNativeWindow",
     L"CWinRTApplicationView", L"v_GetNativeWindow", nullptr,
     &SymbolOffsets::winrtGetNativeWindow},
    {L"XamlAltTabViewHost::Show", L"XamlAltTabViewHost", L"Show", nullptr,
     &SymbolOffsets::xamlAltTabShow},
    {L"ITaskGroupWindowInformation::Position", L"ITaskGroupWindowInformation", L"Position", nullptr,
     &SymbolOffsets::taskGroupPosition},
    {L"XamlAltTabViewHost_CreateInstance", L"XamlAltTabViewHost_CreateInstance", nullptr, nullptr,
     &SymbolOffsets::xamlAltTabCreateInstance},
};

struct EnumContext {
    DWORD64 base{};
    SymbolOffsets* offsets{};
};

bool Contains(std::wstring_view name, const wchar_t* token) {
    return !token || name.find(token) != std::wstring_view::npos;
}

bool LooksLikeVtable(std::wstring_view name) {
    // Undecorated DbgHelp form contains `vftable'. Raw MSVC decoration starts
    // with ??_7. Keep both so the resolver works across PDB/DbgHelp versions.
    return name.find(L"`vftable'") != std::wstring_view::npos ||
           name.find(L"??_7") != std::wstring_view::npos;
}

bool Matches(std::wstring_view name, const Wanted& wanted) {
    if (!Contains(name, wanted.token1) || !Contains(name, wanted.token2) ||
        !Contains(name, wanted.token3)) {
        return false;
    }

    if (wanted.member == &SymbolOffsets::win32ViewVtable ||
        wanted.member == &SymbolOffsets::winrtViewVtable) {
        return LooksLikeVtable(name);
    }

    return true;
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

struct TargetedContext {
    DWORD64 base{};
    const Wanted* wanted{};
    std::uint64_t* slot{};
};

BOOL CALLBACK EnumTargetedSymbol(PSYMBOL_INFOW info, ULONG, PVOID user) {
    auto* ctx = static_cast<TargetedContext*>(user);
    if (*ctx->slot != 0) return FALSE;

    std::wstring_view name(info->Name, info->NameLen);
    if (Matches(name, *ctx->wanted)) {
        *ctx->slot = info->Address - ctx->base;
        return FALSE;
    }
    return TRUE;
}

void ResolveMissingWithTargetedSearch(HANDLE process, DWORD64 base,
                                      SymbolOffsets& offsets) {
    for (const auto& wanted : kWanted) {
        auto& slot = offsets.*(wanted.member);
        if (slot != 0) continue;

        // Search raw/decorated symbols by their stable class or function token.
        // Examples include ??_7CWin32ApplicationView... for vtables.
        std::wstring mask = L"*";
        mask += wanted.token1;
        mask += L"*";

        TargetedContext ctx{base, &wanted, &slot};
        SymEnumSymbolsW(process, base, mask.c_str(), EnumTargetedSymbol, &ctx);
    }
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

    if (enumerated && !Complete(out)) {
        ResolveMissingWithTargetedSearch(process, base, out);
    }

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
