# Session: redcomponent-db-offloading tests wiring sync

Date: 2026-01-17 11:52
Scope: Align the standalone module with tests/CMakeLists wiring from the nested clone.

## Updates
- Copied tests/CMakeLists.txt from the nested redcomponent-db copy.
- Root CMakeLists now delegates to tests subdirectory when BUILD_TESTING is enabled.
- Preserved the original tests wiring session in this repo.

## Files
- CMakeLists.txt
- tests/CMakeLists.txt
- sessions/20260116-21-35-001681-redcomponent-db-offloading-tests-cmake-wiring.md
- sessions/20260117-11-52-002025-redcomponent-db-offloading-tests-wiring-sync.md
