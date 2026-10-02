# Architecture

ScreenTab keeps the native Windows 11 Alt+Tab experience and filters the shell's application-view visibility path while Alt+Tab is being created or shown, including when Windows reuses a cached host.

## Components

### `ScreenTab.exe`

The tray/controller process:

- owns the tray menu and startup preference;
- downloads the matching public Microsoft PDB and resolves the shell symbol RVAs needed by the hook;
- injects `ScreenTabHook.dll` into the current user's Explorer process;
- resolves symbols again and reconnects after Explorer restarts;
- waits for the hook DLL to report that installation actually succeeded;
- retries symbol resolution after temporary failures and shows persistent status in the tray menu;
- fails open if the current Windows build can't be supported safely.

### `ScreenTabHook.dll`

The Explorer-side module:

- hooks the narrow Alt+Tab creation/show/visibility path;
- refreshes the monitor containing the foreground window on every Alt+Tab show, falling back to the cursor monitor;
- lets Windows evaluate each application view normally first;
- hides a view from Alt+Tab only when its native window belongs to another monitor;
- leaves Task View (`Win+Tab`) and unrelated shell visibility checks alone.

The visibility scope is independent of the one-shot positioning scope. Positioning the switcher must not end filtering. The short same-thread grace period is refreshed for every show so delayed foreground-view additions are also filtered.

Before creating Alt+Tab, Windows can transfer foreground to Explorer's hidden `ForegroundStaging` window. That window's monitor does not identify the user's working monitor. Hidden windows, the shell desktop, and known native switcher host classes are excluded from foreground selection, allowing the cursor fallback to select the correct monitor. A normal File Explorer window (`CabinetWClass`) remains a suitable foreground window.

## Why a native hook?

Windows has no supported public API for “show the native Alt+Tab UI, but only for the current monitor.” Earlier prototypes that temporarily changed top-level window styles worked inconsistently because Windows caches Alt+Tab state. ScreenTab instead filters the list inside the shell path used to build the native switcher.

## Compatibility

The shell internals used by Windows can change between builds. ScreenTab resolves symbols for the installed `twinui.pcshell.dll` at runtime instead of using hard-coded offsets. The installer and portable ZIP include Microsoft's x64 DbgHelp, SymSrv, and DIA libraries because the DbgHelp copy built into Windows does not support the symbol server. The resolver uses exact public PDB symbol names and checks that a matching PDB loaded before sharing RVAs with Explorer. On each Explorer process change, the controller resolves the symbols again; the hook compares the loaded image's timestamp, image size, and checksum with the resolved image. The controller treats the hook as working only after the DLL reports successful installation. If resolution or hook installation fails, Windows keeps its normal Alt+Tab behavior and the tray menu shows the recovery status.

## Credits

The filtering strategy and relevant shell symbol names were informed by the open-source Windhawk **Alt+Tab per monitor** mod. ScreenTab is a standalone implementation and does not bundle or require Windhawk.
