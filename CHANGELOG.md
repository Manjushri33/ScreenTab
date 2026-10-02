# Changelog

## v0.1.3

- Show Update and the installed/incoming versions throughout the installer when ScreenTab is already installed.
- Label installing the same version as Reinstall and installing an older version as Downgrade. Ignore stale registration entries with no executable.

## v0.1.2

- Ignore hidden, desktop, and Alt+Tab shell foreground windows when choosing the working monitor; use the cursor monitor instead. This fixes incorrect monitor selection when no File Explorer window is active.
- Refresh the working monitor and filter visibility whenever Alt+Tab is shown, including when Windows reuses a host after its creation grace period.
- Keep visibility filtering active after the switcher has been positioned.

## v0.1.1

- Fix first-run symbol resolution by bundling the Microsoft DbgHelp, SymSrv, and DIA runtime with both downloads.
- Require exact public symbol names and a matching PDB before installing the Alt+Tab hook.
- Reconnect after Explorer restarts, recheck the shell image, and wait for the hook to report that it is ready.
- Retry temporary symbol and hook failures and show recovery status in the tray menu.
- Restore the tray icon when Explorer recreates the taskbar.

## v0.1.0

- Initial public release of native, per-monitor Windows 11 Alt+Tab filtering.
- **Known issue:** The first release used the DbgHelp DLL supplied with Windows, which cannot fetch symbols from Microsoft's symbol server in this setup. Use v0.1.1 or later.
