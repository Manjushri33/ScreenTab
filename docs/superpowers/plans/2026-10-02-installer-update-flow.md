# Direct Update Confirmation Implementation Plan

> **For agentic workers:** Execute inline and request one fresh-context review at the end.

**Goal:** Existing users immediately see installed/incoming versions and an Update button.

**Architecture:** Extend the existing Inno Setup detection, pin the existing folder and startup choice, and skip normal setup choices during maintenance. Verify the real first wizard page rather than only invoking caption callbacks.

**Tech Stack:** Inno Setup 6 Pascal Script, Python harness, GitHub Actions.

---

- [x] Extend `tests/installer/fixtures.iss`, `checks.iss`, and `scripts/test_installer.py` to check page skips, folder/startup preservation, and the actual first wizard page; observe a failure on v0.1.3.
- [x] Update `installer/ScreenTab.iss` to skip setup choices for maintenance, preserve the detected folder and startup task, and put the version transition in the ready-page description. Pass the updated runtime harness.
- [x] Bump release metadata/About/version expectations to 0.1.4 and update README/CHANGELOG. Compile the production installer and run project/native tests with the correct local compiler runtime available.
- [ ] Obtain one read-only review, publish main/tag v0.1.4, verify Actions and downloaded asset versions/checksums, and report the corrected first screen.

Evidence: the initial expanded test failed on the v0.1.3 maintenance welcome flow. The updated harness passes seven scenarios and actual Inno wizard progression for update/startup enabled, update/startup disabled, and fresh setup. Inno creates task controls when preparing wpSelectTasks, so task selection is restored in that page's skip callback using explicit selection/deselection. Silent setup invokes NextButtonClick on its initially skipped welcome; tests permit only that harmless transition and independently prohibit installation in PrepareToInstall. All 15 project contract tests and 13 native hook cases pass, and full installer compilation passes. A guarded probe using real registry/file reads confirms the actual first update screen shows installed 0.1.1, incoming 0.1.4, Update, the existing Program Files directory, and disabled startup. The working app has not been shut down or reinstalled during verification.
