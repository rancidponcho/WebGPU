#include "webgpu-utils.h"

#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#   include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU

#ifdef __EMSCRIPTEN__
#   include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <stdio.h>

/**
 * Callback for WGPUDeviceDescriptor.deviceLostCallback
 */
static void onDeviceLost(const WGPUDevice* device,
                         WGPUDeviceLostReason reason,
                         const char *message,
                         void *pUserData)
{
    (void)device;
    (void)pUserData;

    fprintf(stderr, "Device lost: reason %d", (int)reason);
    if (message) {
        fprintf(stderr, " (%s)", message);
    }
    fprintf(stderr, "\n");
}

/**
 * Error callback invoked whenever there is an error in the use 
 * of the device.
 *
 * NOTE: Put a breakpoint in here.
 */
static void onDeviceError(WGPUErrorType type,
                          const char *message,
                          void *pUserData)
{
    (void)pUserData; // unused

    fprintf(stderr, "Uncaptured device error: type %d\n", (int)type);
    if (message) {
        fprintf(stderr, " (%s)\n", message);
    }
    fprintf(stderr, "\n");
}

/**
 * Callback invoked on COMMAND QUEUE completion
 */
void onQueueWorkDone(WGPUQueueWorkDoneStatus status, 
                     void* pUserData)
{
    (void)pUserData; // unused

    printf("Queued work finished with status: %d\n", (int)status);
}

int main (){
    /**
     * CREATE WebGPU INSTANCE 
     *
     * The instance is the top-level WebGPU object. It represents the
     * connection between this process and whatever GPU backends are
     * available on the system (D3D/Vulkan/Metal/etc).
     *
     * All other WebGPU objects (adapters, devices, queues, etc.) are
     * obtained through this instance.
     */

    // Create a descriptor for the instance
    // Zero-initialize so all fields not explicitly set are 0/NULL.
    WGPUInstanceDescriptor desc = {0};
    desc.nextInChain = NULL;

    /** 
     * By default, Dawn runs callbacks only when the device "ticks", so the 
     * error callbacks are invokes in a different call stack than where the
     * error occurred, making the breakpoint less informative. To force Dawn
     * to invoke error callbacks as soon as there is an error, you can enable
     * an instance toggle
     */
#ifdef WEBGPU_BACKEND_DAWN
    // Make sure the uncaptured error callback is called as soon as an error
    // occurs rather than at the next call to "wgpuDeviceTick".
    WGPUDawnTogglesDescriptor toggles;
    toggles.chain.next = NULL;
    toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
    toggles.disabledToggleCount = 0;
    toggles.enabledToggleCount = 1;
    const char* toggleName = "enable_immediate_error_handling";
    toggles.enabledToggles = &toggleName;

    desc.nextInChain = &toggles.chain;
#endif // WEBGPU_BACKEND_DAWN

    // Create instance
    // NOTE: When targeting Emscripten, the instance descriptor must be NULL
    // because the browser-side implementation ignores it and expects null
#ifdef WEBGPU_BACKEND_EMSCRIPTEN
    WGPUInstance instance = wgpuCreateInstance(NULL);
#else
    WGPUInstance instance = wgpuCreateInstance(&desc);
#endif

    // Verify instance creation
    if (!instance) {
        printf("Could not initialize WebGPU!\n");
        return 1;
    }

    // Display instance pointer (for basic debugging / sanity check)
    printf("WGPU instance: %p\n", (void*)instance);
    
    /**
     * SELECT ADAPTER
     * 
     * An ADAPTER represents a physical or logical GPU on the system.
     * The host may expose multiple adapters (e.g. iGPU + dGPU)
     *
     * The adapter exposes:
     * - supported features
     * - resource/limit values (max bindings, texture sizes, etc.)
     *
     * Those capabiliteis are used to decide:
     * - which rendering path to use in your code
     * - what limits/features to request when creating a DEVICE.
     *
     * Next steps (not implemented here yet):
     *  - fill a WGPURequestAdapterOptions struct
     *  - call requestAdapterSync(instance, &options)
     *  - use the returned WGPUAdapter to create a WGPUDevice
     */
    printf("Requesting adapter...\n");

    WGPURequestAdapterOptions adapterOpts = {0};
    adapterOpts.nextInChain = NULL;
    WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);

    printf("Got adapter: %p\n", (void*)adapter);

    /**
     * DESTROY WebGPU INSTANCE
     * 
     * We no longer need to use the INSTANCE once we have selected the ADAPTER, so
     * it can be released right after the adapter request. The underlying instance
     * object will keep living until the adapter is released.
     */
    wgpuInstanceRelease(instance); 
    
    /** 
     * INSPECT ADAPTER
     * 
     * The ADAPTER object privides information about the underlying 
     * implementation and hardware. It advertises the following:
     *  - Limits:
     *      groups all the maximum and minimum values that may limit the 
     *      behavior of the underlying GPU and its driver, e.g: maximum 
     *      texture size.
     *  - Features: 
     *      non-mandatory extensions that adapters may or may not support.
     *  - Properties:
     *      extra information about the adapter, e.g: name, vendor, etc...
     */
    inspectAdapter(adapter);

    /** 
     * SELECT DEVICE
     *
     * A WebGPU device represents a context of use of the API
     */
    printf("Requesting device...\n");
    
    WGPUDeviceDescriptor deviceDesc = {0}; 
    // minimal device initializion options
    deviceDesc.nextInChain = NULL;
    deviceDesc.label = "My Device"; // anything works here. Used in Error messages
    deviceDesc.requiredFeatureCount = 0; // not requiring any specific features atm
    deviceDesc.requiredLimits = NULL; // might be useful for diagnosing minimal performance
    deviceDesc.defaultQueue.nextInChain = NULL;
    deviceDesc.defaultQueue.label = "The default queue";
    // New device lost callback. deviceLostCallback is deprecated
    deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    deviceDesc.deviceLostCallbackInfo.callback = onDeviceLost;
    deviceDesc.deviceLostCallbackInfo.userdata = NULL;

    
    WGPUDevice device = requestDeviceSync(adapter, &deviceDesc);
    printf("Got device: %p\n", (void*)device);

    // Invoked whenever there is an error in the use of the device
    wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, NULL /* pUserData */);

    /* DESTROY ADAPTER
     *
     * We no longer need the adapter once we have the device.
     */
    wgpuAdapterRelease(adapter);

    // Inspect the adapter
    inspectDevice(device);


    /**
     * CREATE COMMAND QUEUE
     *
     * A WGPUCommandBuffer object cannot be manually created. The buffer uses a
     * special format that is left to the discretion of the user's driver/hardware.
     * A command encoder must be used to create this buffer.
     */
    WGPUQueue queue = wgpuDeviceGetQueue(device);
    // Set COMMAND QUEUE work done callback
    wgpuQueueOnSubmittedWorkDone(queue, onQueueWorkDone, NULL /* pUserData */);
    // Command encoder
    WGPUCommandEncoderDescriptor encoderDesc = {0};
    encoderDesc.nextInChain = NULL;
    encoderDesc.label = "My command encoder";
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

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
    wgpuQueueSubmit(queue, 1, &command);
    wgpuCommandBufferRelease(command);
    printf("Command submitted.\n");
    // Must wait or else we destroy the device before command submission. 
    for (int i = 0; i < 5; ++i) {
        printf("Tick/Poll device...\n");
#if defined(WEBGPU_BACKEND_DAWN)
        wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
        wgpuDevicePoll(device, false, NULL);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
        emscripten_sleep(100);
#endif
    }



    /* DESTROY COMMAND QUEUE
     *
     */
    wgpuQueueRelease(queue);

    /* DESTROY DEVICE
     *
     */
    wgpuDeviceRelease(device);


    return 0;
}
