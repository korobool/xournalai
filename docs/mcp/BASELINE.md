# Upstream baseline (before any MCP code)

Recorded 2026-09-28 on branch `mcp`, starting from upstream commit `b8b3a59ce`, with the fork version bumped to `1.3.7+ai.0.0.1`.

| Item | Result |
|---|---|
| Host | Pop!_OS 22.04 (Ubuntu jammy), GCC 11.4.0, CMake 3.22.1, Ninja |
| Configure | `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=install -DENABLE_GTEST=on -DDOWNLOAD_GTEST=on` |
| Application build | OK (538 steps) — `./build/xournalpp --version` → `1.3.7` |
| Unit and GTK tests | **119/119 passed** (`ninja test-units test-gtk-integration && ctest -j1`) |

## Notes for this machine
- **Use `-DDOWNLOAD_GTEST=on`.** The system `libgtest-dev` (1.11) on jammy is built as C++14 and fails to link
  against C++20 tests (`undefined reference to testing::internal::PrintU8StringTo`).
- **Run `ctest` with `-j1`.** The `ControlLoadHandler.*` save/reload tests share the temp file
  `/tmp/xournalpp-test-units.xopp` and race each other under `ctest -jN`.
- `test-gtk-integration` has to be built explicitly (`ninja test-gtk-integration`). Otherwise ctest reports
  `test-gtk-integration_NOT_BUILT`.
- `php` is not installed, so `src/core/enums/generateConvert.php` can't be run here. Avoid changing `Action.enum.h`,
  or install `php-cli` first.
