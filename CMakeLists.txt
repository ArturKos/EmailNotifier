cmake_minimum_required(VERSION 3.14)
project(EmailNotifier
    VERSION 1.0.0
    DESCRIPTION "Terminal POP3 new-mail notifier"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# -----------------------------------------------------------------------------
# Core library: protocol + persistence. Kept free of ncurses so that tests
# can link against it without pulling in a terminal UI dependency.
# -----------------------------------------------------------------------------
add_library(email_notifier_core STATIC
    src/pop3_client.c
    src/uidl_store.c
)
target_include_directories(email_notifier_core PUBLIC include)

# -----------------------------------------------------------------------------
# Executable: main entry point + ncurses UI.
# -----------------------------------------------------------------------------
find_package(Curses REQUIRED)
add_executable(notifier src/main.c)
target_link_libraries(notifier PRIVATE email_notifier_core ${CURSES_LIBRARIES})
target_include_directories(notifier PRIVATE ${CURSES_INCLUDE_DIR})

# -----------------------------------------------------------------------------
# Tests (GoogleTest via FetchContent).
# -----------------------------------------------------------------------------
option(BUILD_TESTING "Build unit and integration tests" ON)
if(BUILD_TESTING)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(email_notifier_tests
        tests/test_uidl_store.cpp
        tests/test_pop3_client.cpp
    )
    target_link_libraries(email_notifier_tests
        PRIVATE email_notifier_core GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(email_notifier_tests)
endif()

# -----------------------------------------------------------------------------
# Optional: Doxygen target ("cmake --build build --target docs").
# -----------------------------------------------------------------------------
find_package(Doxygen)
if(Doxygen_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/docs/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating Doxygen HTML documentation"
        VERBATIM
    )
endif()
