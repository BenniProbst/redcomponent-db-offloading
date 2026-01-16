enable_testing()

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(test_offloading
    test_offloading.cpp
)

target_link_libraries(test_offloading PRIVATE
    redcomponent-db-offloading
    GTest::gtest_main
)

target_include_directories(test_offloading PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
)

include(GoogleTest)
gtest_discover_tests(test_offloading)
