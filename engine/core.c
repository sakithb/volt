#include <stdio.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include "defines.h"

struct WGPUState state = {};

WGPUBuffer create_buffer(const void* data, size_t size, WGPUBufferUsage usage) {
	WGPUBufferDescriptor desc = {};
	desc.usage = WGPUBufferUsage_CopyDst | usage;
	desc.size  = size;
	WGPUBuffer buffer = wgpuDeviceCreateBuffer(state.device, &desc);
	wgpuQueueWriteBuffer(state.queue, buffer, 0, data, size);
	return buffer;
}

WGPUShaderModule create_shader(const char *path, WGPUDevice device) {
	FILE *f = fopen(path, "rb");
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	char *code = malloc(size);
	fread(code, 1, size, f);
	fclose(f);

	WGPUShaderModuleWGSLDescriptor wgsl = {};
	wgsl.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
	wgsl.code = code;

	WGPUShaderModuleDescriptor desc = {};
	desc.nextInChain = (WGPUChainedStruct*)&wgsl;
	desc.label = NULL;

	return wgpuDeviceCreateShaderModule(device, &desc);
}

static bool redraw() {
	WGPUTextureView backBufView = wgpuSwapChainGetCurrentTextureView(state.swapchain);			// create textureView

	WGPURenderPassColorAttachment colorDesc = {};
	colorDesc.view    = backBufView;
	colorDesc.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
	colorDesc.loadOp  = WGPULoadOp_Clear;
	colorDesc.storeOp = WGPUStoreOp_Store;
	colorDesc.clearValue.r = 0.3f;
	colorDesc.clearValue.g = 0.3f;
	colorDesc.clearValue.b = 0.3f;
	colorDesc.clearValue.a = 1.0f;

	WGPURenderPassDescriptor renderPass = {};
	renderPass.colorAttachmentCount = 1;
	renderPass.colorAttachments = &colorDesc;

	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(state.device, NULL);			// create encoder
	WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPass);	// create pass

	// draw the triangle (comment these five lines to simply clear the screen)
	wgpuRenderPassEncoderSetPipeline(pass, state.pipeline);
	wgpuRenderPassEncoderSetBindGroup(pass, 0, state.bind_grp, 0, 0);
	wgpuRenderPassEncoderSetVertexBuffer(pass, 0, state.vtx_buf, 0, WGPU_WHOLE_SIZE);
	wgpuRenderPassEncoderSetIndexBuffer(pass, state.idx_buf, WGPUIndexFormat_Uint16, 0, WGPU_WHOLE_SIZE);
	wgpuRenderPassEncoderDrawIndexed(pass, 3, 1, 0, 0, 0);

	wgpuRenderPassEncoderEnd(pass);
	wgpuRenderPassEncoderRelease(pass);														// release pass
	WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, NULL);				// create commands
	wgpuCommandEncoderRelease(encoder);														// release encoder

	wgpuQueueSubmit(state.queue, 1, &commands);
	wgpuCommandBufferRelease(commands);														// release commands
	wgpuSwapChainPresent(state.swapchain);
	wgpuTextureViewRelease(backBufView);													// release textureView

	return true;
}

EMSCRIPTEN_KEEPALIVE void setup() {
	state.device = emscripten_webgpu_get_device();
	state.queue = wgpuDeviceGetQueue(state.device);

	WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {};
	canvas_desc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
	canvas_desc.selector = "#canvas";

	WGPUSurfaceDescriptor surf_desc = {};
	surf_desc.nextInChain = (WGPUChainedStruct*)&canvas_desc;

	WGPUSurface surface = wgpuInstanceCreateSurface(NULL, &surf_desc);

	WGPUSwapChainDescriptor swap_desc = {};
	swap_desc.usage  = WGPUTextureUsage_RenderAttachment;
	swap_desc.format = WGPUTextureFormat_BGRA8Unorm;
	swap_desc.width  = 1920;
	swap_desc.height = 1080;
	swap_desc.presentMode = WGPUPresentMode_Fifo;
	
	WGPUSwapChain swapchain = wgpuDeviceCreateSwapChain(state.device, surface, &swap_desc);

	WGPUShaderModule vert_shd = create_shader("default_vert.wgsl", state.device);
	WGPUShaderModule frag_shd = create_shader("default_frag.wgsl", state.device);

	WGPUBufferBindingLayout buf = {};
	buf.type = WGPUBufferBindingType_Uniform;

	WGPUBindGroupLayoutEntry bglEntry = {};
	bglEntry.binding = 0;
	bglEntry.visibility = WGPUShaderStage_Vertex;
	bglEntry.buffer = buf;

	WGPUBindGroupLayoutDescriptor bglDesc = {};
	bglDesc.entryCount = 1;
	bglDesc.entries = &bglEntry;
	WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(state.device, &bglDesc);

	WGPUPipelineLayoutDescriptor layoutDesc = {};
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = &bindGroupLayout;
	WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(state.device, &layoutDesc);

	WGPUVertexAttribute vertAttrs[1] = {};
	vertAttrs[0].format = WGPUVertexFormat_Float32x2;
	vertAttrs[0].offset = 0;
	vertAttrs[0].shaderLocation = 0;

	WGPUVertexBufferLayout vertexBufferLayout = {};
	vertexBufferLayout.arrayStride = 2 * sizeof(float);
	vertexBufferLayout.attributeCount = 1;
	vertexBufferLayout.attributes = vertAttrs;

	WGPUBlendState blend = {};
	blend.color.operation = WGPUBlendOperation_Add;
	blend.color.srcFactor = WGPUBlendFactor_One;
	blend.color.dstFactor = WGPUBlendFactor_One;
	blend.alpha.operation = WGPUBlendOperation_Add;
	blend.alpha.srcFactor = WGPUBlendFactor_One;
	blend.alpha.dstFactor = WGPUBlendFactor_One;

	WGPUColorTargetState colorTarget = {};
	colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
	colorTarget.blend = &blend;
	colorTarget.writeMask = WGPUColorWriteMask_All;

	WGPUFragmentState fragment = {};
	fragment.module = frag_shd;
	fragment.entryPoint = "main";
	fragment.targetCount = 1;
	fragment.targets = &colorTarget;

	WGPURenderPipelineDescriptor desc = {};
	desc.fragment = &fragment;
	desc.layout = pipelineLayout;
	desc.depthStencil = NULL;

	desc.vertex.module = vert_shd;
	desc.vertex.entryPoint = "main";
	desc.vertex.bufferCount = 1;
	desc.vertex.buffers = &vertexBufferLayout;

	desc.multisample.count = 1;
	desc.multisample.mask = 0xFFFFFFFF;
	desc.multisample.alphaToCoverageEnabled = false;

	desc.primitive.frontFace = WGPUFrontFace_CCW;
	desc.primitive.cullMode = WGPUCullMode_None;
	desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
	desc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;

	WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(state.device, &desc);

	wgpuPipelineLayoutRelease(pipelineLayout);

	wgpuShaderModuleRelease(frag_shd);
	wgpuShaderModuleRelease(vert_shd);

	float const vertData[] = {
		-0.8f, -0.8f, // BL
		 0.8f, -0.8f, // BR
		-0.0f,  0.8f, // top
	};

	uint16_t const indxData[] = {
		0, 1, 2,
		0 // padding (better way of doing this?)
	};

	WGPUBuffer vertBuf = create_buffer(vertData, sizeof(vertData), WGPUBufferUsage_Vertex);
	WGPUBuffer indxBuf = create_buffer(indxData, sizeof(indxData), WGPUBufferUsage_Index);

	WGPUBindGroupDescriptor bgDesc = {};
	bgDesc.layout = bindGroupLayout;
	bgDesc.entryCount = 0;
	bgDesc.entries = NULL;

	WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(state.device, &bgDesc);

	wgpuBindGroupLayoutRelease(bindGroupLayout);

	emscripten_request_animation_frame_loop(&redraw, NULL);
}
