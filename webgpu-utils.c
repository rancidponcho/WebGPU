#include "webgpu-utils.h"

#ifdef __EMSCRIPTEN__
#   include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

/**
 * Small struct used to pass data between requestAdapterSync()
 * and the onAdapterRequestEnded() callback.
 *
 * - adapter:   set by the callback when the request succeeds
 * - requestEnded: flag set to true by the callback once the request finishes
 */
typedef struct {
    WGPUAdapter adapter;
    bool requestEnded;
} AdapterRequestData;

/** 
 * Same as AdapterRequstData struction, but use to pass data between
 * and
 */
typedef struct {
    WGPUDevice device;
    bool requestEnded;
} DeviceRequestData;

/**
 * Callback for wgpuInstanceRequestAdapter()
 *
 * This is called asynchronously by the WebGPU implementation when the
 * adapter request completes. The pAdapterRequestData pointer is expected to pointer
 * to a UserAdapterData struct owned by the caller.
 */
static void onAdapterRequestEnded(WGPURequestAdapterStatus status,
                                  WGPUAdapter adapter,
                                  const char *message,
                                  void *pAdapterRequestData)
{
    AdapterRequestData *adapterData = (AdapterRequestData *)pAdapterRequestData;

    if (status == WGPURequestAdapterStatus_Success) {
        adapterData->adapter = adapter;
    } else {
        fprintf(stderr, "Could not get WebGPU adapter: %s\n", message);
    }

    /* signal completion to the waiting code */
    adapterData->requestEnded = true;
}

/**
 * Literally same as onAdapterRequestEnded
 */
static void onDeviceRequestEnded(WGPURequestDeviceStatus status,
                                 WGPUDevice device,
                                 const char *message,
                                 void *pDeviceRequestData)
{
    DeviceRequestData *deviceData = (DeviceRequestData *)pDeviceRequestData;

    if (status == WGPURequestDeviceStatus_Success) {
        deviceData->device = device;
    } else {
        fprintf(stderr, "Could not get WebGPU device: %s\n", message);
    }

    /* signal completion */
    deviceData->requestEnded = true;
}


/**
 * Synchronous wrapper around the asynchronous wgpuInstanceRequestAdapter()
 *
 * The WebGPU C API uses a callback to deliver the adapter. This helper
 * function hides the callback-based interface and instead blocks until
 * the request completes, then returns the resulting WGPUAdapter.
 *
 * Usage:
 *      WGPUAdapter adapter = requestAdapterSync(instance, options);
 */
WGPUAdapter requestAdapterSync(WGPUInstance instance, 
                               const WGPURequestAdapterOptions *options)
{
    AdapterRequestData adapterData;
    adapterData.adapter = NULL;
    adapterData.requestEnded = false;

    /* Start asynchronous adapter request. Result is reported to 
       onAdapterRequestEnded(), which updates adapterData. */
    wgpuInstanceRequestAdapter(
            instance,
            options,
            onAdapterRequestEnded,
            &adapterData       
    );

    /* When using Emscripten, we need to wait until adapterData.requestEnded
       is true before handing control back to the browser. */
#ifdef __EMSCRIPTEN__
    while (!adapterData.requestEnded) {
        emscripten_sleep(100);
    }
#endif // __EMSCRIPTEN__

    assert(adapterData.requestEnded);

    return adapterData.adapter;
}

/** 
 * Utility function to get a WebGPU device. Similar to requestAdapterSync()
 */
WGPUDevice requestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor) 
{
    DeviceRequestData deviceData;
    deviceData.device = NULL;
    deviceData.requestEnded = false;
    
    /* Start asynchronous device request. Result is reported to
       , which updates deviceData. */
    wgpuAdapterRequestDevice(adapter,
                             descriptor,
                             onDeviceRequestEnded,
                             &deviceData
    );

#ifdef __EMSCRIPTEN__
    while (!adapterData.requestEnded) {
        emscripten_sleep(100);
    }
#endif // __EMSCRIPTEN__

    assert(deviceData.requestEnded);

    return deviceData.device;
}


/** 
 * INSPECT ADAPTER
 * 
 * Query and print adapter features and limits.
 */
void inspectAdapter(WGPUAdapter adapter)
{
#ifndef __EMSCRIPTEN__
    WGPUSupportedLimits supportedLimits = {0};
    supportedLimits.nextInChain = NULL;

#ifdef WEBGPU_BACKEND_DAWN
    bool success = wgpuAdapterGetLimits(adapter, &supportedLimits) == WGPUStatus_Success;
#else
    bool success = wgpuAdapterGetLimits(adapter, &supportedLimits);
#endif

if (success) {
    printf("Adapter limits:\n");
    printf(" - maxTextureDimension1D: %"PRIu32"\n", supportedLimits.limits.maxTextureDimension1D);
    printf(" - maxTextureDimension2D: %"PRIu32"\n", supportedLimits.limits.maxTextureDimension2D);
    printf(" - maxTextureDimension3D: %"PRIu32"\n", supportedLimits.limits.maxTextureDimension3D);
    printf(" - maxTextureArrayLayers: %"PRIu32"\n", supportedLimits.limits.maxTextureArrayLayers);
}
#endif // NOT __EMSCRIPTEN__

    // Call the function a fist time with a null return address, just to get
    // the entry count.
    size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, NULL);

    WGPUFeatureName *features = malloc(featureCount * sizeof *features);
    if (!features) {
        fprintf(stderr, "Failed to allocate ADAPTER feature list (count=%zu)\n", featureCount);
        return;
    }

    // Call the function a second time, with non-null return address
    wgpuAdapterEnumerateFeatures(adapter, features);  
    
    // Display Adapter features. See build-dawn/_deps/dawn-build/gen/include/dawn/webgpu.h 
    // for feature names.
    printf("Adapter features:\n");
    for (size_t i = 0; i < featureCount; ++i) {
        WGPUFeatureName f = features[i];
        printf(" - 0x%x\n", (unsigned)f);
    }

    free(features);

    // Properties
    WGPUAdapterProperties properties = {0};
    properties.nextInChain = NULL;
    wgpuAdapterGetProperties(adapter, &properties);

    printf("Adapter properties:\n");
    printf(" - vendorID: %"PRIu32"\n", properties.vendorID);
    if (properties.vendorName) {
        printf(" - vendorName: %s\n", properties.vendorName);
    }
    if (properties.architecture) {
        printf(" - architecture: %s\n", properties.architecture);
    }    
    printf(" - deviceID: %"PRIu32"\n", properties.deviceID);
    if (properties.driverDescription) {
        printf(" - driverDescription: %s\n", properties.driverDescription);
    }
    printf(" - adapterType: 0x%x\n", properties.adapterType);
    printf(" - backendType: 0x%x\n", properties.backendType);    
}

/**
 * INSPECT DEVICE
 *
 * Basically the same as inspectAdapter, but for device
 */
void inspectDevice(WGPUDevice device)
{
    size_t featureCount = wgpuDeviceEnumerateFeatures(device, NULL);

    WGPUFeatureName *features = malloc(featureCount * sizeof *features);
    if (!features) {
        fprintf(stderr, "Failed to allocate DEVICE feature list (count=%zu)\n", featureCount);
        return;
    }

    wgpuDeviceEnumerateFeatures(device, features);  
    
    printf("Device features:\n");
    for (size_t i = 0; i < featureCount; ++i) {
        WGPUFeatureName f = features[i];
        printf(" - 0x%x\n", (unsigned)f);
    }

    free(features);

    WGPUSupportedLimits supportedLimits = {0};
    supportedLimits.nextInChain = NULL;

#ifdef WEBGPU_BACKEND_DAWN
    bool success = wgpuDeviceGetLimits(device, &supportedLimits) == WGPUStatus_Success;
#else
    bool success = wgpuDeviceGetLimits(device, &supportedLimits);
#endif

    if (success) {
        printf("Device limits:\n");
        printf(" - maxTextureDimension1D: %"PRIu32"\n", supportedLimits.limits.maxTextureDimension1D);
        printf(" - maxTextureDimension2D: %"PRIu32"\n", supportedLimits.limits.maxTextureDimension2D);
        printf(" - maxTextureDimension3D: %"PRIu32"\n", supportedLimits.limits.maxTextureDimension3D);
        printf(" - maxTextureArrayLayers: %"PRIu32"\n", supportedLimits.limits.maxTextureArrayLayers);
    }
}

/**
 * CREATE WGPU Surface
 *
 * platform specific WebGPU surface creation for SDL
 */
WGPUSurface create_wgpu_surface(WGPUInstance instance, SDL_Window* window) {
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    WGPUSurfaceDescriptor surface_desc = { .label = "Cross-Platform Surface" };

#if defined(SDL_PLATFORM_WIN32)
    // WINDOWS (DirectX/Vulkan)
    WGPUSurfaceDescriptorFromWindowsHWND win_desc = {
        .chain = { .next = NULL, .sType = WGPUSType_SurfaceDescriptorFromWindowsHWND },
        .hinstance = GetModuleHandle(NULL),
        .hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL)
    };
    surface_desc.nextInChain = &win_desc.chain;

#elif defined(SDL_PLATFORM_APPLE)
    // MACOS / IOS (Metal)
    // SDL3 handles the CAMetalLayer for you via SDL_PROP_WINDOW_APPLE_VIEW_POINTER
    WGPUSurfaceDescriptorFromMetalLayer metal_desc = {
        .chain = { .next = NULL, .sType = WGPUSType_SurfaceDescriptorFromMetalLayer },
        .layer = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_APPLE_VIEW_POINTER, NULL)
    };
    surface_desc.nextInChain = &metal_desc.chain;

#elif defined(SDL_PLATFORM_LINUX)
    // LINUX (X11 or Wayland)
    // We check which driver SDL is currently using
    const char* driver = SDL_GetCurrentVideoDriver();

    if (SDL_strcmp(driver, "wayland") == 0) {
        WGPUSurfaceDescriptorFromWaylandSurface wayland_desc = {
            .chain = { .next = NULL, .sType = WGPUSType_SurfaceDescriptorFromWaylandSurface },
            .display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL),
            .surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL)
        };
        surface_desc.nextInChain = &wayland_desc.chain;
    } else {
        // Fallback to X11
        WGPUSurfaceDescriptorFromXlibWindow x11_desc = {
            .chain = { .next = NULL, .sType = WGPUSType_SurfaceDescriptorFromXlibWindow },
            .display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL),
            .window = (uint32_t)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)
        };
        surface_desc.nextInChain = &x11_desc.chain;
    }
#endif

    return wgpuInstanceCreateSurface(instance, &surface_desc);
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
 *
 * INITIALIZE WebGPU
 */

bool initWebGPU(Context* context)
{
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
        return NULL;
    }

    // Display instance pointer (for basic debugging / sanity check)
    printf("WGPU instance: %p\n", (void*)instance);

    
    /**
     * Create WGPU Surface
     */
    context->surface = create_wgpu_surface(instance, context->window);    

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
     * Capabilities:
     * - which rendering path to use in your code
     * - what limits/features to request when creating a DEVICE.
     *
     * Next steps (not implemented here yet):
     *  - fill a WGPURequestAdapterOptions struct
     *  - call requestAdapterSync(instance, &options)
     *  - use the returned WGPUAdapter to create a WGPUDevice
     */
    printf("Requesting adapter...\n");
    
    WGPURequestAdapterOptions adapterOpts = {
        .compatibleSurface = context->surface,
        .nextInChain = NULL
    };
    WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);

    printf("Got adapter: %p\n", (void*)adapter);
    inspectAdapter(adapter);

    /**
     * DESTROY WebGPU INSTANCE
     * 
     * We no longer need to use the INSTANCE once we have selected the ADAPTER, so
     * it can be released right after the adapter request. The underlying instance
     * object will keep living until the adapter is released.
     */
    wgpuInstanceRelease(instance); 
 
    /** 
     * SELECT DEVICE
     *
     * A WebGPU device represents a context of use of the API
     */
    printf("Requesting device...\n");
    
    WGPUDeviceDescriptor deviceDesc = {0}; 
    deviceDesc.nextInChain = NULL;
    // minimal device initializion options
    deviceDesc.label = "My Device"; // sed in error messages / debugging
    deviceDesc.requiredFeatureCount = 0; // no optional features required
    deviceDesc.requiredLimits = NULL; // use implmentation defaults
    deviceDesc.defaultQueue.nextInChain = NULL;
    deviceDesc.defaultQueue.label = "The default queue";
    // New device lost callback. deviceLostCallback is deprecated
    deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    deviceDesc.deviceLostCallbackInfo.callback = onDeviceLost;
    deviceDesc.deviceLostCallbackInfo.userdata = NULL;

    
    context->device = requestDeviceSync(adapter, &deviceDesc);
    printf("Got device: %p\n", (void*)context->device);

    // Invoked whenever there is an error in the use of the device
    wgpuDeviceSetUncapturedErrorCallback(context->device, onDeviceError, NULL /* pUserData */);

    /* DESTROY ADAPTER
     *
     * We no longer need the adapter once we have the device.
     */
    wgpuAdapterRelease(adapter);

    // Configure device Surface
    WGPUSurfaceConfiguration config = {
        .device = context->device,
        .format = WGPUTextureFormat_BGRA8Unorm, // Or get preferred format from adapter
        .usage = WGPUTextureUsage_RenderAttachment,
        .width = kScreenWidth,
        .height = kScreenHeight,
        .presentMode = WGPUPresentMode_Fifo
    };
    wgpuSurfaceConfigure(context->surface, &config);

    // Inspect the adapter
    inspectDevice(context->device);

    /**
     * CREATE COMMAND QUEUE
     *
     *
     *
     *
     */
    context->queue = wgpuDeviceGetQueue(context->device);

    if (!context->queue) {
        fprintf(stderr, "Failed to get queue\n");
        return false;
    }
    
    wgpuQueueOnSubmittedWorkDone(context->queue, onQueueWorkDone, NULL);

    return true;
}

