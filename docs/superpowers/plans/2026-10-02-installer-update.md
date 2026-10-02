# Visible Installer Updates Implementation Plan

> **For agentic workers:** Execute this short plan inline. Use one fresh-context review at completion.

**Goal:** Clearly identify updates to an existing ScreenTab installation throughout the installer.

**Architecture:** Keep the existing Inno Setup identity and file replacement behavior. Add registry-based detection and version-specific wizard captions, with a separate test harness exercising the production Pascal code and real wizard controls.

**Tech Stack:** Inno Setup 6, Pascal Script, Python unittest, GitHub Actions.

---

- [x] Add `scripts/test_installer.py` and Pascal fixtures under `tests/installer/`; compile a harness with the production code, substitute only external registry/file inputs, and assert its wizard captions. Run against the existing installer to observe the missing Update behavior.
- [x] Extend `installer/ScreenTab.iss`: show the welcome page, read HKLM64's matching uninstall record, distinguish update/reinstall/downgrade numerically, and change welcome, ready, installing, and finished controls. Keep ordinary installations and unrelated page captions intact. Run the harness again.
- [x] Bump the release to 0.1.3 in `VERSION`, `CMakeLists.txt`, installer metadata, controller About text, and existing version expectations. Document the visible update in README and CHANGELOG. Add the harness to the build workflow after Inno Setup becomes available.
- [x] Run Python contract tests, native hook tests, the installer harness, and compile the production installer. Request one read-only review and resolve any important findings.
- [x] Commit the reviewed change, push main and a new v0.1.3 tag under the user's existing GitHub-update authorization, wait for CI, and verify the release downloads and checksums.

Verification: seven runtime installer scenarios passed. The initial harness failed on the missing Update title before implementation. All 15 project contract tests and 13 native hook cases passed. Full installer compilation passed with local build-artifact paths; a read-only probe using actual HKLM64 and file-system reads detected this PC's installed 0.1.1 and showed Update to 0.1.3. Independent review found no critical or important issues. Actual installation is not part of the verification, so the existing working app remains running.

Published code commit `6f4d1717bcc52f85768b87fa83ec45baea42ed47` and tag `v0.1.3`. GitHub Actions run `37006741131` completed successfully, including full installer compilation and the seven runtime UI scenarios. Downloaded release assets have matching SHA-256 checksums; both installer and controller report file version 0.1.3.0, and the portable ZIP contains the six expected files. GitHub's latest release is v0.1.3.
