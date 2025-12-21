#include "global.h"
#include "webgpu-utils.h"


#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#   include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_system.h> // for native handles

#ifdef __EMSCRIPTEN__
#   include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <stdio.h>


const uint32_t kScreenWidth = 640;
const uint32_t kScreenHeight = 480;

/*
 * SDL SHIT
 *
 *
 *
 */

SDL_Window* gWindow = NULL;

SDL_Window* createSDLWindow() {
    SDL_Window* window = NULL;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        return false; // Exit early if Init fails
    }

    window = SDL_CreateWindow("Learn WebGPU", kScreenWidth, kScreenHeight, 0);
    if (window == NULL) {
        SDL_Log("Window could not be created! SDL Error: %s\n", SDL_GetError());
        SDL_Quit();
    }
    
    return window;
}

void closeSDL(Context* context)
{
    SDL_DestroyWindow(context->window);
    context->window = NULL;

    SDL_Quit();
}

/**
 * Initialize WEB GPU
 *
 *
 *
 */
bool initApp(Context* context)
{
    context->window = createSDLWindow();
    if (!context->window) return false;

    if (!initWebGPU(context)) return false;

    return true;
}

void closeContext(Context* context)
{
    wgpuQueueRelease(context->queue);
    wgpuDeviceRelease(context->device);
    closeSDL(context);
}


int main ()
{

    /**
     * Initialize App
     */
    Context context = {0};
    initApp(&context);





    // Command encoder
    WGPUCommandEncoderDescriptor encoderDesc = {0};
    encoderDesc.nextInChain = NULL;
    encoderDesc.label = "My command encoder";
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(context.device, &encoderDesc);

    // Debug placeholder for encoder instruction (no object to manipulate yet)
    wgpuCommandEncoderInsertDebugMarker(encoder, "Do one thing");
    wgpuCommandEncoderInsertDebugMarker(encoder, "Do another thing");

    // Generate command buffer using encoder
    WGPUCommandBufferDescriptor cmdBufferDescriptor = {0};
    cmdBufferDescriptor.nextInChain = NULL;
    cmdBufferDescriptor.label = "Command buffer";
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);

    // Release encoder
    wgpuCommandEncoderRelease(encoder); 

    // Finally, submit command queue
    printf("Submitting command...\n");
    wgpuQueueSubmit(context.queue, 1, &command);
    wgpuCommandBufferRelease(command);
    printf("Command submitted.\n");
    // Must wait or else we destroy the device before command submission. 
    for (int i = 0; i < 5; ++i) {
        printf("Tick/Poll device...\n");
#if defined(WEBGPU_BACKEND_DAWN)
        wgpuDeviceTick(context.device);
#elif defined(WEBGPU_BACKEND_WGPU)
        wgpuDevicePoll(device, false, NULL);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
        emscripten_sleep(100);
#endif
    }




    // main loop
    while(1)
    {
    }

    closeContext(&context);

    return 0;
}
