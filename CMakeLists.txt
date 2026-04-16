cmake_minimum_required(VERSION 3.16)
project(ChildUsageTracker VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch nlohmann/json ────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# ── Executable (WIN32 = /SUBSYSTEM:WINDOWS — no console window) ───────────────
add_executable(ChildUsageTracker WIN32
    src/main.cpp
    src/config.cpp
    src/process_monitor.cpp
    src/tracker.cpp
    src/gist_client.cpp
    src/dialogs.cpp
)

target_include_directories(ChildUsageTracker PRIVATE src)

target_link_libraries(ChildUsageTracker PRIVATE
    nlohmann_json::nlohmann_json
    Winhttp.lib
    Shell32.lib
    comctl32.lib
)

target_compile_definitions(ChildUsageTracker PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0601   # Windows 7+ (required for TaskDialogIndirect)
)

# Warnings as errors for cleaner code
if(MSVC)
    target_compile_options(ChildUsageTracker PRIVATE /W3)
endif()
