#include "controller/controller.h"
#include <dbghelp.h>
#include <windows.h>
#include <array>
#include <vector>
#include <string_view>

#pragma comment(lib, "dbghelp.lib")

namespace mat {
namespace {
struct Wanted {
    const wchar_t* needle;
    std::uint64_t SymbolOffsets::* member;
};

constexpr Wanted kWanted[] = {
    {L"CVirtualDesktop::IsViewVisible(struct IApplicationView *,int *)", &SymbolOffsets::virtualDesktopIsViewVisible},
    {L"CWin32ApplicationView::`vftable'{for `IApplicationView'}", &SymbolOffsets::win32ViewVtable},
    {L"CWin32ApplicationView::v_GetNativeWindow(struct HWND__ * *)", &SymbolOffsets::win32GetNativeWindow},
    {L"CWinRTApplicationView::`vftable'{for `IApplicationView'}", &SymbolOffsets::winrtViewVtable},
    {L"CWinRTApplicationView::v_GetNativeWindow(struct HWND__ * *)", &SymbolOffsets::winrtGetNativeWindow},
    {L"XamlAltTabViewHost::Show(struct IImmersiveMonitor *,enum ALT_TAB_VIEW_FLAGS,struct IApplicationView *)", &SymbolOffsets::xamlAltTabShow},
    {L"ITaskGroupWindowInformation>::Position(struct winrt::Windows::Foundation::Rect const &)", &SymbolOffsets::taskGroupPosition},
    {L"XamlAltTabViewHost_CreateInstance(struct XamlViewHostInitializeArgs const &,struct _GUID const &,void * *)", &SymbolOffsets::xamlAltTabCreateInstance},
};

struct EnumContext {
    DWORD64 base{};
    SymbolOffsets* offsets{};
};

BOOL CALLBACK EnumSymbols(PSYMBOL_INFOW info, ULONG, PVOID user) {
    auto* ctx = static_cast<EnumContext*>(user);
    std::wstring_view name(info->Name, info->NameLen);
    for (const auto& wanted : kWanted) {
        auto& slot = ctx->offsets->*(wanted.member);
        if (slot != 0) continue;
        if (name.find(wanted.needle) != std::wstring_view::npos) {
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
}

bool ResolveTwinuiSymbols(SymbolOffsets& out, std::wstring& error) {
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
    if (!SymEnumSymbolsW(process, base, nullptr, EnumSymbols, &ctx) || !Complete(out)) {
        error = L"Required Alt+Tab symbols were not found for this Windows build";
        SymUnloadModule64(process, base);
        SymCleanup(process);
        return false;
    }

    SymUnloadModule64(process, base);
    SymCleanup(process);
    return true;
}
}
