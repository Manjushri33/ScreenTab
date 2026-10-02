# Direct update confirmation

The v0.1.3 installer detects an existing installation correctly, but still sends users through the normal destination/start-menu/task pages. The user's screenshot shows the Update title on the ordinary destination page, where no versions are visible. This does not meet their intended update experience.

For a registered installation with an existing executable, show the Ready to Update confirmation as the first wizard page. Its heading, version description, summary, and Update button must make the existing and incoming versions visible. Skip Welcome, destination, start-menu, and task selection for update/reinstall/downgrade. Preserve the registered installation directory and the current user's Start with Windows state. New and stale installations retain the normal setup flow and editable folder/task options.

Keep the existing maintenance progress/completion labels and file replacement mechanism. The interface remains English. Release as v0.1.4 under the existing publishing authorization.

Test both the production page-skip decisions and the actual first page visited by Inno's wizard engine. The test harness always cancels before installation. Cover enabled/disabled startup preservation, a custom existing folder, and normal fresh/stale installation behavior.
