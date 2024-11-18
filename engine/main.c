#include <stdio.h>
#include <stdbool.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include "app.h"
#include "setup.h"

struct App app = {0};

void main_loop() {
	// Get texture view

	WGPUSurfaceTexture tex = {0};
	wgpuSurfaceGetCurrentTexture(app.surface, &tex);

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
	WGPUCommandEncoder cenc = wgpuDeviceCreateCommandEncoder(app.device, &cenc_desc);

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
	
	wgpuRenderPassEncoderSetPipeline(rpass, app.pipeline);
	wgpuRenderPassEncoderSetVertexBuffer(rpass, 0, app.vbuf, 0, wgpuBufferGetSize(app.vbuf));
	wgpuRenderPassEncoderSetIndexBuffer(rpass, app.ibuf, WGPUIndexFormat_Uint32, 0, wgpuBufferGetSize(app.ibuf));
	wgpuRenderPassEncoderDraw(rpass, 3, 1, 0, 0);
	
	// End drawing

	wgpuRenderPassEncoderEnd(rpass);
	wgpuRenderPassEncoderRelease(rpass);

	WGPUCommandBufferDescriptor cbuf_desc = {0};
	WGPUCommandBuffer cbuf = wgpuCommandEncoderFinish(cenc, &cbuf_desc);
	wgpuCommandEncoderRelease(cenc);

	wgpuQueueSubmit(app.queue, 1, &cbuf);
	wgpuCommandBufferRelease(cbuf);

	// Frame cleanup

	wgpuTextureViewRelease(view);
}

int main() {
	setup();
	emscripten_set_main_loop(main_loop, 0, true);
	cleanup();

	return 0;
}

