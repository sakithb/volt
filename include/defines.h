#ifndef DEFINES_H
#define DEFINES_H

#include <emscripten/html5_webgpu.h>

struct WGPUState {
	WGPUDevice device;
	WGPUQueue queue;
	WGPUSwapChain swapchain;

	WGPURenderPipeline pipeline;
	WGPUBuffer vtx_buf;
	WGPUBuffer idx_buf;
	WGPUBindGroup bind_grp;
};

#endif
