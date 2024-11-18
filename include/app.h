#ifndef APP_H
#define APP_H

#include <emscripten/html5_webgpu.h>

struct App {
	WGPUSurface surface;
	WGPUDevice device;
	WGPURenderPipeline pipeline;
	WGPUQueue queue;

	WGPUBuffer vbuf;
	WGPUBuffer ibuf;

	int width;
	int height;
};

#endif
