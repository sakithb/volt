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

	fseek(f, 0, SEEK_END);
	long s = ftell(f);

	char *code = malloc(s);
	fread(code, 1, s, f);

	fclose(f);

	WGPUShaderModuleWGSLDescriptor wgsl = {};
	wgsl.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
	wgsl.code = code;

	WGPUShaderModuleDescriptor desc = {};
	desc.nextInChain = (WGPUChainedStruct*)&wgsl;
	desc.label = NULL;

	return wgpuDeviceCreateShaderModule(rs.device, &desc);
}

static void request_adapter_cb(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, void *userdata) {
	if (status == WGPURequestAdapterStatus_Success) {
		rs.adapter = adapter;
	} else {
		fprintf(stderr, "Failed to request adapter: %s\n", message);
	}
}

static void request_device_cb(WGPURequestDeviceStatus status, WGPUDevice device, char const *message, void *userdata) {
	if (status == WGPURequestDeviceStatus_Success) {
		rs.device = device;
	} else {
		fprintf(stderr, "Failed to request device: %s\n", message);
	}
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

	// Render to view

	WGPUCommandEncoderDescriptor cenc_desc = {0};
	WGPUCommandEncoder cenc = wgpuDeviceCreateCommandEncoder(rs.device, &cenc_desc);

	WGPURenderPassColorAttachment cattach = {0};
	cattach.view = view;
	cattach.loadOp = WGPULoadOp_Clear;
	cattach.storeOp = WGPUStoreOp_Store;
	cattach.clearValue = (WGPUColor){1.0f, 1.0f, 1.0f, 1.0f};
	cattach.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

	WGPURenderPassDescriptor rpass_desc = {0};
	rpass_desc.colorAttachments = &cattach;
	rpass_desc.colorAttachmentCount = 1;

	WGPURenderPassEncoder rpass = wgpuCommandEncoderBeginRenderPass(cenc, &rpass_desc);
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

	WGPUDeviceDescriptor dev_desc = {0};
	wgpuAdapterRequestDevice(rs.adapter, &dev_desc, request_device_cb, NULL);

	while (rs.device == NULL) {
		emscripten_sleep(100);
	}

	rs.queue = wgpuDeviceGetQueue(rs.device);

	// Use canvas as surface

	WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {0};
	canvas_desc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
	canvas_desc.selector = "#canvas";

	WGPUSurfaceDescriptor surf_desc = {0};
	surf_desc.nextInChain = (WGPUChainedStruct*)&canvas_desc;

	rs.surface = wgpuInstanceCreateSurface(rs.instance, &surf_desc);

	WGPUSurfaceConfiguration surf_cfg = {0};
	surf_cfg.device = rs.device;
	surf_cfg.format = wgpuSurfaceGetPreferredFormat(rs.surface, rs.adapter);
	surf_cfg.usage = WGPUTextureUsage_RenderAttachment;
	surf_cfg.alphaMode = WGPUCompositeAlphaMode_Auto;
	surf_cfg.width = width;
	surf_cfg.height = height;
	surf_cfg.presentMode = WGPUPresentMode_Fifo;

	wgpuSurfaceConfigure(rs.surface, &surf_cfg);

	emscripten_set_main_loop(main_loop, 0, true);

	// Cleanup

	wgpuSurfaceUnconfigure(rs.surface);
	wgpuSurfaceRelease(rs.surface);

	wgpuQueueRelease(rs.queue);
	wgpuDeviceRelease(rs.device);
	wgpuAdapterRelease(rs.adapter);

	return 0;
}

