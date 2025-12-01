#ifndef WEBGPU_UTILS_H
#define WEBGPU_UTILS_H

#include <webgpu/webgpu.h>

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
                               const WGPURequestAdapterOptions *options);

/** 
 * Utility function to get a WebGPU device. Similar to requestAdapterSync()
 */
WGPUDevice requestDeviceSync(WGPUAdapter adapter, 
                             WGPUDeviceDescriptor const * descriptor); 

/** 
 * INSPECT ADAPTER
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
void inspectAdapter(WGPUAdapter adapter);

/**
 * Basically the same as inspectAdapter, see comments for differences.
 */
void inspectDevice(WGPUDevice device);

#endif // WEBGPU_UTILS_H
