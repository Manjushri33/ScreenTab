#pragma once
#include <windows.h>
#include <cstdint>

namespace mat {
inline constexpr wchar_t kSharedMappingName[] = L"Local\\ScreenTab.Shared.v2";
inline constexpr std::uint32_t kProtocolVersion = 2;

enum class HookStatus : LONG {
    Pending = 0,
    Ready = 1,
    ModuleMismatch = 2,
    InstallFailed = 3,
};

struct ModuleIdentity {
    DWORD timeDateStamp{};
    DWORD imageSize{};
    DWORD checkSum{};
};

struct SymbolOffsets {
    std::uint64_t virtualDesktopIsViewVisible{};
    std::uint64_t win32ViewVtable{};
    std::uint64_t win32GetNativeWindow{};
    std::uint64_t winrtViewVtable{};
    std::uint64_t winrtGetNativeWindow{};
    std::uint64_t xamlAltTabShow{};
    std::uint64_t taskGroupPosition{};
    std::uint64_t xamlAltTabCreateInstance{};
};

struct SharedState {
    std::uint32_t protocolVersion{kProtocolVersion};
    volatile LONG enabled{1};
    volatile LONG symbolsReady{0};
    volatile LONG hookStatus{static_cast<LONG>(HookStatus::Pending)};
    DWORD hookInitThreadId{};
    DWORD explorerPid{};
    ModuleIdentity moduleIdentity{};
    SymbolOffsets symbols{};
};
}
