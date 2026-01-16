# Session: redcomponent-db-offloading tests CMake wiring

Date: 2026-01-16 21:35
Scope: Move test wiring into tests/CMakeLists.txt for consistent discovery.

## Updates
- Moved GoogleTest setup and test target definition into tests/CMakeLists.txt.
- Root CMakeLists now delegates to the tests subdirectory when BUILD_TESTING is enabled.

## Files
- CMakeLists.txt
- tests/CMakeLists.txt
- sessions/20260116-21-35-001681-redcomponent-db-offloading-tests-cmake-wiring.md
