#pragma once
#include <windows.h>
#include <cstdint>

namespace mat {
inline constexpr wchar_t kSharedMappingName[] = L"Local\\ScreenTab.Shared.v1";
inline constexpr std::uint32_t kProtocolVersion = 1;

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
    DWORD explorerPid{};
    SymbolOffsets symbols{};
};
}
