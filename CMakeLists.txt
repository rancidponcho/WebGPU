cmake_minimum_required(VERSION 3.0...3.25)

project(
    LearnWebGPU
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =========================
# Global configuration
# =========================

# C standard (for C sources)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(SMAKE_CXX_STANDARD_REQUIRED ON)

# =========================
# Dependencies
# =========================

include(FetchContent)

# --- SDL3 ---
# We disable shared libs to make the final executable more portable (static linking)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
# Optional: Disable uneccessary SDL features
set(SDL_TEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

# webgpu / Dawn lives in the 'webgpu' subdirectory
add_subdirectory(webgpu)

# =========================
# App target
# =========================
add_executable(App 
    main.c
    webgpu-utils.c
)

# Link against the webgpu target
# Note: SDL3::SDL3-static is used if you want to be explicit,
# but SDL3::SDL3 usually aliases to the correct one.
target_link_libraries(App PRIVATE 
    webgpu
    SDL3::SDL3
)

# Copy wgpu runtime binaries (DLL / SO / etc.) next to the executable.
# For Dawn, this is a no-op because there are no precompiled binaries.
target_copy_webgpu_binaries(App)


# Link as C++ so std:: stuff resolves
set_property(TARGET App PROPERTY LINKER_LANGUAGE CXX)

# Recommended properties: C++ standard, no extensions, warnings as errors
set_target_properties(App PROPERTIES
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

# =========================
# Warnings
# =========================

if (MSVC)
    target_compile_options(App PRIVATE /W4)
else()
    target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
endif()

# =========================
# Platform / generator specifics
# =========================

# Generate schemes for Xcode and enable GPU frame capture
if (XCODE)
    set_target_properties(App PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
    )
endif()

# Emscripten builds should emit an .html file instead of WASM module
if (EMSCRIPTEN)
    set_target_properties(App PROPERTIES SUFFIX ".html")
    # Enable the use of emscripten_sleep()
    target_link_options(App PRIVATE -sASYNCIFY)
endif()

