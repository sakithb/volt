#ifndef CORE_H
#define CORE_H

#include <emscripten/html5_webgpu.h>

struct RendererState {
	WGPUInstance instance;
	WGPUAdapter adapter;
	WGPUDevice device;
	WGPUQueue queue;
	WGPUSurface surface;
	WGPURenderPipeline pipeline;
};

#endif
