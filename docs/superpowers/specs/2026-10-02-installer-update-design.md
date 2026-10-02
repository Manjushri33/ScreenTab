# Visible installer updates

The user requested that an existing ScreenTab installation be visibly treated as an update. Keep the existing Inno Setup installer and application identity, and read its registered installation directory and DisplayVersion from the 64-bit machine uninstall entry. Ignore stale entries whose executable is absent.

Show the existing and incoming versions on the welcome page and the ready page. Use Update in the window title, confirmation button, progress, and completion text. The same version uses Reinstall; an older incoming version uses Downgrade so the operation is accurately described. A fresh installation keeps the normal Setup wording. Keep the previous installation directory selected through Inno Setup's existing UsePreviousAppDir behavior.

The interface stays English, consistent with the current installer. There is no automatic updater and no new application setting. Publish a new patch release rather than replacing the existing v0.1.2 assets.

Verify real Pascal code and wizard control captions with an Inno Setup test harness that supplies controlled registry/file responses and cancels before installation. Cover fresh install, older/same/newer versions, missing version, stale registration, and preservation of normal page labels. Compile the complete production installer and run existing native and project tests.
