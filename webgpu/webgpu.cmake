
cmake_minimum_required(VERSION 3.24) # 3.24+ adds better FetchContent support
project(MyWebGPUProject)

# --- Dawn Build Configuration ---
# These MUST be set before FetchContent_MakeAvailable to override Dawn's defaults
set(DAWN_FETCH_DEPENDENCIES ON CACHE BOOL "" FORCE)
set(DAWN_BUILD_PROTOBUF OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_NODE_BINDINGS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_AST_FUZZER OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_IR_FUZZER OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

# Use standard FetchContent (no more deprecated manual download macros)
include(FetchContent)

FetchContent_Declare(
    dawn
    GIT_REPOSITORY https://dawn.googlesource.com/dawn
    GIT_TAG        chromium/6842 # Using a stable Chromium-branch tag is safer than 'main'
    GIT_SHALLOW    ON
)

# This replaces the old "MakeAvailable" logic and applies our flags
FetchContent_MakeAvailable(dawn)

# --- Your Application ---
add_executable(${PROJECT_NAME} main.cpp)

# Link against dawn::webgpu_cpp (the C++ wrapper) or dawn::dawn_native
# Note: Dawn's targets are namespaced
target_link_libraries(${PROJECT_NAME} PRIVATE dawn::webgpu_cpp dawn::dawn_native)

# Set C++ standard (Dawn requires C++17 or 20)
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
