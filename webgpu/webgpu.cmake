cmake_minimum_required(VERSION 3.24)
project(WebGPUProject)

# 1. Selection Logic
set(WEBGPU_BACKEND "DAWN" CACHE STRING "Backend implementation: EMSCRIPTEN, WGPU, or DAWN")
string(TOUPPER ${WEBGPU_BACKEND} WEBGPU_BACKEND_U)

include(FetchContent)

# ------------------------------------------------------------------------------
# 2. DAWN BACKEND (Official Google Repo)
# ------------------------------------------------------------------------------
if (WEBGPU_BACKEND_U STREQUAL "DAWN")
    # Force flags to avoid Protobuf/Fuzzers before fetching
    set(DAWN_FETCH_DEPENDENCIES ON CACHE BOOL "" FORCE)
    set(DAWN_BUILD_PROTOBUF OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
    set(TINT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        dawn
        GIT_REPOSITORY https://dawn.googlesource.com/dawn
        GIT_TAG        chromium/6842
        GIT_SHALLOW    ON
    )
    FetchContent_MakeAvailable(dawn)
    
    set(WEBGPU_LIBRARIES dawn::webgpu_cpp dawn::dawn_native)

# ------------------------------------------------------------------------------
# 3. WGPU-NATIVE BACKEND (Official wgpu-native)
# ------------------------------------------------------------------------------
elseif (WEBGPU_BACKEND_U STREQUAL "WGPU")
    FetchContent_Declare(
        wgpu_native
        GIT_REPOSITORY https://github.com/gfx-rs/wgpu-native.git
        GIT_TAG        v0.19.4.1 # Or your preferred version
        GIT_SHALLOW    ON
    )
    FetchContent_MakeAvailable(wgpu_native)
    set(WEBGPU_LIBRARIES wgpu_native)

# ------------------------------------------------------------------------------
# 4. EMSCRIPTEN BACKEND
# ------------------------------------------------------------------------------
elseif (EMSCRIPTEN OR WEBGPU_BACKEND_U STREQUAL "EMSCRIPTEN")
    # Emscripten provides WebGPU headers natively; no fetch usually required 
    # but we can add an interface library if needed for consistency.
    message(STATUS "Using Emscripten built-in WebGPU")
endif()

# ------------------------------------------------------------------------------
# 5. Application Setup
# ------------------------------------------------------------------------------
add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE ${WEBGPU_LIBRARIES})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
