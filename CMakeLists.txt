cmake_minimum_required(VERSION 3.20)
project(redcomponent-db-offloading VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Main library (header-only)
add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Installation
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)

# Testing
option(BUILD_TESTING "Build tests" ON)

if(BUILD_TESTING)
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
        tests/test_offloading.cpp
    )

    target_link_libraries(test_offloading PRIVATE
        ${PROJECT_NAME}
        GTest::gtest_main
    )

    target_include_directories(test_offloading PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    include(GoogleTest)
    gtest_discover_tests(test_offloading)

    # Add test to CTest
    add_test(NAME test_offloading COMMAND test_offloading)
endif()
