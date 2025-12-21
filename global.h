#ifndef GLOBAL_H
#define GLOBAL_H

#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#   include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU

#include <SDL3/SDL.h>

typedef struct{
    SDL_Window* window;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
} Context;

extern const uint32_t kScreenWidth;
extern const uint32_t kScreenHeight;

#endif // GLOBAL_H
