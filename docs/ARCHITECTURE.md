# Architecture

ScreenTab keeps the native Windows 11 Alt+Tab experience and filters the shell's application-view visibility path only while Alt+Tab is being created.

## Components

### `ScreenTab.exe`

The tray/controller process:

- owns the tray menu and startup preference;
- resolves the private Windows shell symbol RVAs needed by the hook;
- injects `ScreenTabHook.dll` into the current user's Explorer process;
- reconnects after Explorer restarts;
- fails open if the current Windows build can't be supported safely.

### `ScreenTabHook.dll`

The Explorer-side module:

- hooks the narrow Alt+Tab creation/visibility path;
- finds the monitor containing the foreground window, falling back to the cursor monitor;
- lets Windows evaluate each application view normally first;
- hides a view from Alt+Tab only when its native window belongs to another monitor;
- leaves Task View (`Win+Tab`) and unrelated shell visibility checks alone.

## Why a native hook?

Windows has no supported public API for “show the native Alt+Tab UI, but only for the current monitor.” Earlier prototypes that temporarily changed top-level window styles worked inconsistently because Windows caches Alt+Tab state. ScreenTab instead filters the list inside the shell path used to build the native switcher.

## Compatibility

The shell internals used by Windows can change between builds. ScreenTab resolves symbols for the installed `twinui.pcshell.dll` at runtime instead of using hard-coded offsets. If resolution fails, the hook is not installed and Windows keeps its normal Alt+Tab behavior.

## Credits

The filtering strategy and relevant shell symbol names were informed by the open-source Windhawk **Alt+Tab per monitor** mod. ScreenTab is a standalone implementation and does not bundle or require Windhawk.
