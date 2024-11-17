#include <stdio.h>
#include <stdbool.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include "core.h"

const int width = 1920;
const int height = 1080;

struct RendererState rs = {0};

WGPUShaderModule create_shader(const char *path) {
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		fprintf(stderr, "Failed to read shader\n");
		abort();
	}

	fseek(f, 0, SEEK_END);
	long sz = ftell(f);

	char *code = malloc(sz);
	fseek(f, 0, SEEK_SET);
	fread(code, 1, sz-1, f);

	fclose(f);

	WGPUShaderModuleWGSLDescriptor wgsl = {0};
	wgsl.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
	wgsl.code = code;

	WGPUShaderModuleDescriptor desc = {0};
	desc.nextInChain = &wgsl.chain;

	WGPUShaderModule sh = wgpuDeviceCreateShaderModule(rs.device, &desc);
	free(code);

	return sh;
}

static void request_adapter_cb(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, void *userdata) {
	if (status == WGPURequestAdapterStatus_Success) {
		rs.adapter = adapter;
	} else {
		fprintf(stderr, "Failed to request adapter: %s\n", message);
		abort();
	}
}

static void request_device_cb(WGPURequestDeviceStatus status, WGPUDevice device, char const *message, void *userdata) {
	if (status == WGPURequestDeviceStatus_Success) {
		rs.device = device;
	} else {
		fprintf(stderr, "Failed to request device: %s\n", message);
		abort();
	}
}

static void device_error_cb(WGPUErrorType type, char const* message, void* user_data) {
	fprintf(stderr, "Uncaptured error: %d: %s\n", type, message);
}

void main_loop() {
	// Get texture view

	WGPUSurfaceTexture tex = {0};
	wgpuSurfaceGetCurrentTexture(rs.surface, &tex);

	if (tex.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
		fprintf(stderr, "Failed to retrieve texture\n");
		return;
	}

	WGPUTextureViewDescriptor view_desc = {0};
	view_desc.format = wgpuTextureGetFormat(tex.texture);
	view_desc.dimension = WGPUTextureViewDimension_2D;
	view_desc.baseMipLevel = 0;
	view_desc.mipLevelCount = 1;
	view_desc.baseArrayLayer = 0;
	view_desc.arrayLayerCount = 1;
	view_desc.aspect = WGPUTextureAspect_All;

	WGPUTextureView view = wgpuTextureCreateView(tex.texture, &view_desc);

	wgpuTextureRelease(tex.texture);

	// Create render pass

	WGPUCommandEncoderDescriptor cenc_desc = {0};
	WGPUCommandEncoder cenc = wgpuDeviceCreateCommandEncoder(rs.device, &cenc_desc);

	WGPURenderPassColorAttachment cattach = {0};
	cattach.view = view;
	cattach.loadOp = WGPULoadOp_Clear;
	cattach.storeOp = WGPUStoreOp_Store;
	cattach.clearValue = (WGPUColor){0.0f, 0.0f, 0.0f, 1.0f};
	cattach.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

	WGPURenderPassDescriptor rpass_desc = {0};
	rpass_desc.colorAttachments = &cattach;
	rpass_desc.colorAttachmentCount = 1;

	WGPURenderPassEncoder rpass = wgpuCommandEncoderBeginRenderPass(cenc, &rpass_desc);
	
	// Begin drawing
	
	wgpuRenderPassEncoderSetPipeline(rpass, rs.pipeline);
	wgpuRenderPassEncoderDraw(rpass, 3, 1, 0, 0);
	
	// End drawing

	wgpuRenderPassEncoderEnd(rpass);
	wgpuRenderPassEncoderRelease(rpass);

	WGPUCommandBufferDescriptor cbuf_desc = {0};
	WGPUCommandBuffer cbuf = wgpuCommandEncoderFinish(cenc, &cbuf_desc);
	wgpuCommandEncoderRelease(cenc);

	wgpuQueueSubmit(rs.queue, 1, &cbuf);
	wgpuCommandBufferRelease(cbuf);

	// Frame cleanup

	wgpuTextureViewRelease(view);
}

int main() {
	// Create a instance, adapter and device

	rs.instance = wgpuCreateInstance(NULL);

	WGPURequestAdapterOptions adap_opt = {0};
	wgpuInstanceRequestAdapter(rs.instance, &adap_opt, request_adapter_cb, NULL);

	while (rs.adapter == NULL) {
		emscripten_sleep(100);
	}

	wgpuInstanceRelease(rs.instance);

	WGPUDeviceDescriptor dev_desc = {0};
	wgpuAdapterRequestDevice(rs.adapter, &dev_desc, request_device_cb, NULL);

	while (rs.device == NULL) {
		emscripten_sleep(100);
	}

	wgpuDeviceSetUncapturedErrorCallback(rs.device, device_error_cb, NULL);

	rs.queue = wgpuDeviceGetQueue(rs.device);

	// Use canvas as surface
	
	WGPUTextureFormat surface_fmt = wgpuSurfaceGetPreferredFormat(rs.surface, rs.adapter);

	WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {0};
	canvas_desc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
	canvas_desc.selector = "#canvas";

	WGPUSurfaceDescriptor surf_desc = {0};
	surf_desc.nextInChain = &canvas_desc.chain;

	rs.surface = wgpuInstanceCreateSurface(rs.instance, &surf_desc);

	WGPUSurfaceConfiguration surf_cfg = {0};
	surf_cfg.device = rs.device;
	surf_cfg.format = surface_fmt;
	surf_cfg.usage = WGPUTextureUsage_RenderAttachment;
	surf_cfg.alphaMode = WGPUCompositeAlphaMode_Auto;
	surf_cfg.width = width;
	surf_cfg.height = height;
	surf_cfg.presentMode = WGPUPresentMode_Fifo;

	wgpuSurfaceConfigure(rs.surface, &surf_cfg);

	// Create pipeline
	
	WGPUShaderModule shader = create_shader("assets/default.wgsl");
	if (shader == NULL) {
		fprintf(stderr, "Failed to create shader\n");
		return 1;
	}
	
	WGPURenderPipelineDescriptor pipeline_desc = {0};

	pipeline_desc.vertex.module = shader;
	pipeline_desc.vertex.entryPoint = "vs_main";
	pipeline_desc.vertex.constants = NULL;
	pipeline_desc.vertex.constantCount = 0;
	pipeline_desc.vertex.buffers = NULL;
	pipeline_desc.vertex.bufferCount = 0;

	pipeline_desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
	pipeline_desc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
	pipeline_desc.primitive.frontFace = WGPUFrontFace_CCW;
	pipeline_desc.primitive.cullMode = WGPUCullMode_None;

	WGPUBlendState blend_state = {0};
	blend_state.color.srcFactor = WGPUBlendFactor_SrcAlpha;
	blend_state.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
	blend_state.color.operation = WGPUBlendOperation_Add;
	blend_state.alpha.srcFactor = WGPUBlendFactor_Zero;
	blend_state.alpha.dstFactor = WGPUBlendFactor_One;
	blend_state.alpha.operation = WGPUBlendOperation_Add;
	
	WGPUColorTargetState ctarget = {0};
	ctarget.format = surface_fmt;
	ctarget.blend = &blend_state;
	ctarget.writeMask = WGPUColorWriteMask_All;

	WGPUFragmentState frag_state = {0};
	frag_state.module = shader;
	frag_state.entryPoint = "fs_main";
	frag_state.constants = NULL;
	frag_state.constantCount = 0;
	frag_state.targets = &ctarget;
	frag_state.targetCount = 1;

	pipeline_desc.fragment = &frag_state;

	pipeline_desc.multisample.count = 1;
	pipeline_desc.multisample.mask = ~0u;
	pipeline_desc.multisample.alphaToCoverageEnabled = false;

	pipeline_desc.layout = NULL;

	rs.pipeline = wgpuDeviceCreateRenderPipeline(rs.device, &pipeline_desc);

	// Mainloop

	emscripten_set_main_loop(main_loop, 0, true);

	// Cleanup

	wgpuRenderPipelineRelease(rs.pipeline);
	wgpuSurfaceUnconfigure(rs.surface);
	wgpuSurfaceRelease(rs.surface);

	wgpuShaderModuleRelease(shader);

	wgpuQueueRelease(rs.queue);
	wgpuDeviceRelease(rs.device);
	wgpuAdapterRelease(rs.adapter);

	return 0;
}

