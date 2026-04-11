cmake_minimum_required(VERSION 3.22)
project(connman_nc CXX C)

set(CMAKE_CXX_STANDARD  23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── sdbus-cpp v2 as submodule ────────────────────────────────────────────────
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDBUSCPP_BUILD_CODEGEN OFF CACHE BOOL "" FORCE)
set(SDBUSCPP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/sdbus-cpp EXCLUDE_FROM_ALL)

# ── dart_api_dl — compile as plain C ────────────────────────────────────────
add_library(dart_api_dl OBJECT src/dart_api_dl.c)
target_include_directories(dart_api_dl PUBLIC include)

# ── Header-only types (Glaze + Connman structs) ──────────────────────────────
add_library(connman_types INTERFACE)
target_include_directories(connman_types INTERFACE include)

# ── Connman Manager library ───────────────────────────────────────────────────
add_library(connman_manager STATIC
    src/connman_manager.cpp
    $<TARGET_OBJECTS:dart_api_dl>
)

target_include_directories(connman_manager
    PUBLIC include
    PUBLIC generated
)

target_link_libraries(connman_manager PUBLIC sdbus-c++ pthread)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()