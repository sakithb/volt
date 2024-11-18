#include <stdio.h>
#include <stdlib.h>
#include <emscripten/html5_webgpu.h>
#include "shared.h"
#include "shader.h"

WGPUShaderModule create_shader(const char *path) {
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		fprintf(stderr, "Failed to read shader\n");
		abort();
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);

	char *code = malloc(size);
	fseek(f, 0, SEEK_SET);
	fread(code, 1, size - 1, f);

	fclose(f);

	WGPUShaderModuleWGSLDescriptor wgsl = {0};
	wgsl.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
	wgsl.code = code;

	WGPUShaderModuleDescriptor desc = {0};
	desc.nextInChain = &wgsl.chain;

	WGPUShaderModule shader = wgpuDeviceCreateShaderModule(app.device, &desc);
	if (shader == NULL) {
		fprintf(stderr, "Failed to create shader\n");
		abort();
	}

	free(code);

	return shader;
}
