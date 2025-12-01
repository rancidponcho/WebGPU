#include "webgpu-utils.h"

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
        fprintf(stderr, "Failed to allocate ADAPTER feature list (count=%zu)\n", featureCount);
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

/**
 * Basically the same as inspectAdapter
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
