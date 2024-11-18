#ifndef SHADER_H
#define SHADER_H

#include <emscripten/html5_webgpu.h>

WGPUShaderModule create_shader(const char *path);

#endif
