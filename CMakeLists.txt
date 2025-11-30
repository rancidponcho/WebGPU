cmake_minimum_required(VERSION 3.0...3.25)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(
    LearnWebGPU
    VERSION 0.1.0
    LANGUAGES C CXX
)

# =========================
# Global configuration
# =========================

# C standard (for C sources)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =========================
# Dependencies
# =========================

# webgpu / Dawn lives in the 'webgpu' subdirectory
add_subdirectory(webgpu)

# =========================
# App target
# =========================
add_executable(App main.c)

# Link against the webgpu target
target_link_libraries(App PRIVATE webgpu)

# Copy wgpu runtime binaries (DLL / SO / etc.) next to the executable.
# For Dawn, this is a no-op because there are no precompiled binaries.
target_copy_webgpu_binaries(App)


# Link as C++ so std:: stuff resolves
set_property(TARGET App PROPERTY LINKER_LANGUAGE CXX)

# Recommended properties: C++ standard, no extensions, warnings as errors
set_target_properties(App PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
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

