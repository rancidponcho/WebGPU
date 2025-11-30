#include <webgpu/webgpu.h>

#ifdef __EMSCRIPTEN__
#   include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>
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
} UserData;

/**
 * Callback for wgpuInstanceRequestAdapter()
 *
 * This is called asynchronously by the WebGPU implementation when the
 * adapter request completes. The pUserData pointer is expected to pointer
 * to a UserData struct owned by the caller.
 */
static void onAdapterRequestEnded(WGPURequestAdapterStatus status,
                                  WGPUAdapter adapter,
                                  const char *message,
                                  void *pUserData)
{
    UserData *userData = (UserData *)pUserData;

    if (status == WGPURequestAdapterStatus_Success) {
        userData->adapter = adapter;
    } else {
        fprintf(stderr, "Could not get WebGPU adapter: %s\n", message);
    }

    /* signal completion to the waiting code */
    userData->requestEnded = true;
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
    UserData userData;
    userData.adapter = NULL;
    userData.requestEnded = false;

    /* Start asynchronous adapter request. Result is reported to 
       onAdapterRequestEnded(), which updates userData. */
    wgpuInstanceRequestAdapter(
            instance,
            options,
            onAdapterRequestEnded,
            &userData       
    );

    /* Busy-wait until the callback marks the request as finished.
       Some backends may require pumping events here, e.g.:
       wgpuInstanceProcessEvents(instance); */
    while (!userData.requestEnded) {
        // wgpuInstanceProcessEvents(instance);
    }

    /* When using Emscripten, we need to hand the control back to the 
       browser until the adapter is ready. */
#ifdef __EMSCRIPTEN__
    while (!userData.requestEnded) {
        emscripten_sleep(100);
    }
#endif // __EMSCRIPTEN__

    assert(userData.requestEnded);

    return userData.adapter;
}

/** INSPECT ADAPTER
 * 
 * The ADAPTER object privides information about the underlying 
 * implementation and hardware. It advertises the following:
 *  - Limits:
 *      regroups all the maximum and minimum values that may limit the 
 *      behavior of the underlying GPU and its driver, e.g: maximum 
 *      texture size.
 *  - Features: 
 *      non-mandatory extensions that adapters may or may not support.
 *  - Properties:
 *      extra information about the adapter, e.g: name, vendor, etc...
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
        fprintf(stderr, "Failed to allocate feature list (count=%zu)\n", featureCount);
        return;
    }

    // Call the function a second time, with non-null return address
    wgpuAdapterEnumerateFeatures(adapter, features);  
    
    // Display Adapter features. Consort to
    // build-dawn/_deps/dawn-build/gen/include/dawn/webgpu.h for
    // feature names.
    printf("Adapter features:\n");
    for (size_t i = 0; i < featureCount; ++i) {
        WGPUFeatureName f = features[i];
        printf(" - 0x%x\n", (unsigned)f);
    }

    // Properties
    WGPUAdapterProperties properties = {0};
    properties.nextInChain = NULL;
    wgpuAdapterGetProperties(adapter, &properties);

    printf("Adapter properties:");
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
        // handle error
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
    printf("Requesting adapter...");

    WGPURequestAdapterOptions adapterOpts = {0};
    adapterOpts.nextInChain = NULL;
    WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);

    printf("Got adapter: %p\n", (void*)adapter);

    /* DESTROY WebGPU INSTANCE
     * 
     * We no longer need to use the INSTANCE once we have selected our
     * ADAPTER, so we can call wgpuInstanceRelease() right after the
     * adapter request instead of at the very end. The underlying instance
     * object will keep on living until the adapter gets released but we
     * do not need to manage this.
     */
    wgpuInstanceRelease(instance); 

    /** INSPECT ADAPTER
     * 
     * The ADAPTER object privides information about the underlying 
     * implementation and hardware. It advertises the following:
     *  - Limits:
     *      regroups all the maximum and minimum values that may limit the 
     *      behavior of the underlying GPU and its driver, e.g: maximum 
     *      texture size.
     *  - Features: 
     *      non-mandatory extensions that adapters may or may not support.
     *  - Properties:
     *      extra information about the adapter, e.g: name, vendor, etc...
     */
    inspectAdapter(adapter);

    /* DESTROY ADAPTER
     *
     */
    wgpuAdapterRelease(adapter);


    return 0;
}
