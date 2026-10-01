#include "controller/controller.h"
#include <dbghelp.h>
#include <windows.h>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>

#pragma comment(lib, "dbghelp.lib")

namespace mat {
namespace {
struct Wanted {
    const wchar_t* label;
    const wchar_t* decoratedName;
    std::uint64_t SymbolOffsets::* member;
};

// Public PDB names include the complete signature and interface-specific
// vtable name. Partial names can match compiler-generated dtor$ symbols.
constexpr Wanted kWanted[] = {
    {L"CVirtualDesktop::IsViewVisible",
     L"?IsViewVisible@CVirtualDesktop@@UEAAJPEAUIApplicationView@@PEAH@Z",
     &SymbolOffsets::virtualDesktopIsViewVisible},
    {L"CWin32ApplicationView IApplicationView vtable",
     L"??_7CWin32ApplicationView@@6BIApplicationView@@@",
     &SymbolOffsets::win32ViewVtable},
    {L"CWin32ApplicationView::v_GetNativeWindow",
     L"?v_GetNativeWindow@CWin32ApplicationView@@EEAAJPEAPEAUHWND__@@@Z",
     &SymbolOffsets::win32GetNativeWindow},
    {L"CWinRTApplicationView IApplicationView vtable",
     L"??_7CWinRTApplicationView@@6BIApplicationView@@@",
     &SymbolOffsets::winrtViewVtable},
    {L"CWinRTApplicationView::v_GetNativeWindow",
     L"?v_GetNativeWindow@CWinRTApplicationView@@EEAAJPEAPEAUHWND__@@@Z",
     &SymbolOffsets::winrtGetNativeWindow},
    {L"XamlAltTabViewHost::Show",
     L"?Show@XamlAltTabViewHost@@UEAAJPEAUIImmersiveMonitor@@W4ALT_TAB_VIEW_FLAGS@@PEAUIApplicationView@@@Z",
     &SymbolOffsets::xamlAltTabShow},
    {L"ITaskGroupWindowInformation::Position",
     L"?Position@?$consume_Windows_Internal_Shell_TaskGroups_ITaskGroupWindowInformation@UITaskGroupWindowInformation@TaskGroups@Shell@Internal@Windows@winrt@@@impl@winrt@@QEBA@AEBURect@Foundation@Windows@3@@Z",
     &SymbolOffsets::taskGroupPosition},
    {L"XamlAltTabViewHost_CreateInstance",
     L"?XamlAltTabViewHost_CreateInstance@@YAJAEBUXamlViewHostInitializeArgs@@AEBU_GUID@@PEAPEAX@Z",
     &SymbolOffsets::xamlAltTabCreateInstance},
};

struct EnumContext {
    DWORD64 base{};
    SymbolOffsets* offsets{};
    bool duplicate{};
    bool invalidAddress{};
};

BOOL CALLBACK EnumSymbols(PSYMBOL_INFOW info, ULONG, PVOID user) {
    auto* ctx = static_cast<EnumContext*>(user);
    const std::wstring_view name(info->Name);
    for (const auto& wanted : kWanted) {
        if (name != wanted.decoratedName) continue;
        if (info->Address <= ctx->base) {
            ctx->invalidAddress = true;
            continue;
        }
        const std::uint64_t rva = info->Address - ctx->base;
        auto& slot = ctx->offsets->*(wanted.member);
        if (slot != 0 && slot != rva) ctx->duplicate = true;
        else slot = rva;
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

bool CheckSymbolRuntime(std::wstring& error) {
    const auto appDirectory = GetExecutableDirectory();
    for (const wchar_t* file : {L"dbghelp.dll", L"symsrv.dll", L"msdia140.dll"}) {
        if (!std::filesystem::exists(appDirectory / file)) {
            error = L"Symbol runtime is incomplete: ";
            error += file;
            error += L" is missing. Reinstall ScreenTab";
            return false;
        }
    }
    const HMODULE dbghelp = GetModuleHandleW(L"dbghelp.dll");
    wchar_t loadedPath[MAX_PATH]{};
    if (!dbghelp || !GetModuleFileNameW(dbghelp, loadedPath, MAX_PATH)) {
        error = L"Could not identify the loaded DbgHelp library";
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::equivalent(loadedPath, appDirectory / L"dbghelp.dll", ec)) {
        error = L"ScreenTab loaded a different DbgHelp library";
        return false;
    }
    return true;
}
}

bool ResolveTwinuiSymbols(SymbolOffsets& out, std::wstring& error) {
    out = {};
    error.clear();
    if (!CheckSymbolRuntime(error)) return false;

    wchar_t systemDir[MAX_PATH]{};
    if (!GetSystemDirectoryW(systemDir, MAX_PATH)) {
        error = L"GetSystemDirectory failed";
        return false;
    }
    const auto modulePath = std::filesystem::path(systemDir) / L"twinui.pcshell.dll";

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_PUBLICS_ONLY | SYMOPT_DEFERRED_LOADS |
                  SYMOPT_FAIL_CRITICAL_ERRORS);

    wchar_t localAppData[MAX_PATH]{};
    if (!ExpandEnvironmentStringsW(L"%LOCALAPPDATA%", localAppData, MAX_PATH)) {
        error = L"Could not locate the symbol cache directory";
        return false;
    }
    const auto cache = std::filesystem::path(localAppData) / L"ScreenTab" / L"symbols";
    std::error_code ec;
    std::filesystem::create_directories(cache, ec);
    if (ec) {
        error = L"Could not create the symbol cache directory";
        return false;
    }
    const std::wstring symbolPath =
        L"srv*" + cache.wstring() + L"*https://msdl.microsoft.com/download/symbols";

    if (!SymInitializeW(process, symbolPath.c_str(), FALSE)) {
        error = L"Could not initialize Microsoft symbol resolver";
        return false;
    }
    const DWORD64 base = SymLoadModuleExW(
        process, nullptr, modulePath.c_str(), nullptr, 0, 0, nullptr, 0);
    if (!base) {
        error = L"Could not load twinui.pcshell.dll into the symbol resolver";
        SymCleanup(process);
        return false;
    }

    EnumContext ctx{base, &out};
    const BOOL enumerated = SymEnumSymbolsW(process, base, nullptr, EnumSymbols, &ctx);
    const DWORD enumerationError = enumerated ? ERROR_SUCCESS : GetLastError();

    IMAGEHLP_MODULEW64 moduleInfo{};
    moduleInfo.SizeOfStruct = sizeof(moduleInfo);
    const BOOL hasModuleInfo = SymGetModuleInfoW64(process, base, &moduleInfo);
    const bool hasPdb = hasModuleInfo &&
        (moduleInfo.SymType == SymPdb || moduleInfo.SymType == SymDia) &&
        !moduleInfo.PdbUnmatched;

    if (!enumerated) {
        error = L"Could not enumerate twinui.pcshell.dll symbols (error " +
                std::to_wstring(enumerationError) + L")";
    } else if (!hasPdb) {
        error = L"Microsoft PDB for twinui.pcshell.dll was not loaded. "
                L"Check the network connection and symbol server";
    } else if (ctx.duplicate || ctx.invalidAddress) {
        error = L"Ambiguous or invalid Alt+Tab symbol addresses for this Windows build";
    } else if (!Complete(out)) {
        error = L"Required Alt+Tab symbols were not found for this Windows build. "
                L"Missing symbols: " + MissingSymbols(out);
    } else {
        for (const auto& wanted : kWanted) {
            if (out.*(wanted.member) >= moduleInfo.ImageSize) {
                error = L"Alt+Tab symbol address lies outside twinui.pcshell.dll";
                break;
            }
        }
    }

    SymUnloadModule64(process, base);
    SymCleanup(process);
    if (!error.empty()) {
        out = {};
        return false;
    }
    return true;
}
}
