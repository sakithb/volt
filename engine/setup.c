#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include "shared.h"
#include "shader.h"
#include "setup.h"

static struct WGPURequiredLimits device_required_limits();

static void request_adapter_cb(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, void *userdata);
static void request_device_cb(WGPURequestDeviceStatus status, WGPUDevice device, char const *message, void *userdata);
static void device_lost_cb(enum WGPUDeviceLostReason reason, const char *message, void *userdata);
static void device_error_cb(WGPUErrorType type, const char *message, void *userdata);

void setup() {
	app.width = 1920;
	app.height = 1080;

	WGPUInstance instance = wgpuCreateInstance(NULL);

	// Surface

	WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {0};
	canvas_desc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
	canvas_desc.selector = "#canvas";

	WGPUSurfaceDescriptor surf_desc = {0};
	surf_desc.nextInChain = &canvas_desc.chain;

	app.surface = wgpuInstanceCreateSurface(instance, &surf_desc);

	// Adapter

	WGPURequestAdapterOptions adap_opts = {0};
	adap_opts.compatibleSurface = app.surface;

	WGPUAdapter adapter;
	wgpuInstanceRequestAdapter(instance, &adap_opts, request_adapter_cb, &adapter);
	while (adapter == NULL) {
		emscripten_sleep(100);
	}

	wgpuInstanceRelease(instance);

	// Device
	
	WGPURequiredLimits dev_limits = device_required_limits();
	WGPUDeviceDescriptor dev_desc = {0};
	dev_desc.deviceLostCallback = device_lost_cb;
	dev_desc.requiredLimits = &dev_limits;

	wgpuAdapterRequestDevice(adapter, &dev_desc, request_device_cb, &app.device);
	while (app.device == NULL) {
		emscripten_sleep(100);
	}

	wgpuDeviceSetUncapturedErrorCallback(app.device, device_error_cb, NULL);

	// Configure surface
	
	WGPUTextureFormat surf_fmt = wgpuSurfaceGetPreferredFormat(app.surface, adapter);

	WGPUSurfaceConfiguration surf_cfg = {0};
	surf_cfg.width = app.width;
	surf_cfg.height = app.height;
	surf_cfg.usage = WGPUTextureUsage_RenderAttachment;
	surf_cfg.format = surf_fmt;
	surf_cfg.device = app.device;
	surf_cfg.presentMode = WGPUPresentMode_Fifo;
	surf_cfg.alphaMode = WGPUCompositeAlphaMode_Auto;

	wgpuSurfaceConfigure(app.surface, &surf_cfg);

	wgpuAdapterRelease(adapter);

	// Pipeline
	
	WGPUShaderModule shader = create_shader("assets/default.wgsl");

	WGPUVertexAttribute vattrs[] = {
		(WGPUVertexAttribute){
			.offset = 0,
			.format = WGPUVertexFormat_Float32x2,
			.shaderLocation = 0,
		},
		(WGPUVertexAttribute){
			.offset = 2 * sizeof(float),
			.format = WGPUVertexFormat_Float32x3,
			.shaderLocation = 1,
		},
	};

	WGPUVertexBufferLayout vbuf_layouts[] = {
		(WGPUVertexBufferLayout) {
			.attributes = vattrs,
			.attributeCount = sizeof(vattrs) / sizeof(WGPUVertexAttribute),
			.arrayStride = 5 * sizeof(float),
			.stepMode = WGPUVertexStepMode_Vertex,
		},
	};

	WGPUBlendState blend_state = {0};
	blend_state.color.srcFactor = WGPUBlendFactor_SrcAlpha;
	blend_state.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
	blend_state.color.operation = WGPUBlendOperation_Add;
	blend_state.alpha.srcFactor = WGPUBlendFactor_Zero;
	blend_state.alpha.dstFactor = WGPUBlendFactor_One;
	blend_state.alpha.operation = WGPUBlendOperation_Add;

	WGPUColorTargetState col_tgt_state = {0};
	col_tgt_state.format = surf_fmt;
	col_tgt_state.blend = &blend_state;
	col_tgt_state.writeMask = WGPUColorWriteMask_All;

	WGPUFragmentState frag_state = {0};
	frag_state.module = shader;
	frag_state.entryPoint = "fs_main";
	frag_state.targets = &col_tgt_state;
	frag_state.targetCount = 1;

	WGPURenderPipelineDescriptor pipeline_desc = {0};

	pipeline_desc.vertex.buffers = vbuf_layouts;
	pipeline_desc.vertex.bufferCount = sizeof(vbuf_layouts) / sizeof(WGPUVertexBufferLayout);
	pipeline_desc.vertex.module = shader;
	pipeline_desc.vertex.entryPoint = "vs_main";

	pipeline_desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
	pipeline_desc.primitive.frontFace = WGPUFrontFace_CCW;
	pipeline_desc.primitive.cullMode = WGPUCullMode_None;

	pipeline_desc.fragment = &frag_state;

	pipeline_desc.multisample.count = 1;
	pipeline_desc.multisample.mask = ~0u;
	pipeline_desc.multisample.alphaToCoverageEnabled = false;

	app.pipeline = wgpuDeviceCreateRenderPipeline(app.device, &pipeline_desc);

	wgpuShaderModuleRelease(shader);

	// Buffers
	
	app.queue = wgpuDeviceGetQueue(app.device);

	float vertices[] = {
		+0.0f, +0.5f, 1.0f, 0.0f, 0.0f,
		-0.25f, -0.5f, 0.0f, 1.0f, 0.0f,
		+0.25f, -0.5f, 0.0f, 0.0f, 1.0f,
	};

	uint32_t indices[] = {
		0, 1, 2,
	};

	WGPUBufferDescriptor vbuf_desc = {0};
	vbuf_desc.size = sizeof(vertices);
	vbuf_desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
	vbuf_desc.mappedAtCreation = false;

	app.vbuf = wgpuDeviceCreateBuffer(app.device, &vbuf_desc);
	wgpuQueueWriteBuffer(app.queue, app.vbuf, 0, vertices, sizeof(vertices));

	WGPUBufferDescriptor ibuf_desc = {0};
	ibuf_desc.size = sizeof(indices);
	ibuf_desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
	ibuf_desc.mappedAtCreation = false;

	app.ibuf = wgpuDeviceCreateBuffer(app.device, &ibuf_desc);
	wgpuQueueWriteBuffer(app.queue, app.ibuf, 0, indices, sizeof(indices));
}

void cleanup() {
	wgpuSurfaceRelease(app.surface);
	wgpuDeviceRelease(app.device);
	wgpuRenderPipelineRelease(app.pipeline);
	wgpuQueueRelease(app.queue);
}

static struct WGPURequiredLimits device_required_limits() {
	// TODO
	
	WGPURequiredLimits limits = {0};

	// Defaults
	limits.limits.maxTextureDimension1D = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxTextureDimension2D = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxTextureDimension3D = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxTextureArrayLayers = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxBindGroups = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxBindGroupsPlusVertexBuffers = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxBindingsPerBindGroup = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxDynamicUniformBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxDynamicStorageBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxSampledTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxSamplersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxStorageBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxStorageTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxUniformBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxUniformBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED;
	limits.limits.maxStorageBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED;
	limits.limits.minUniformBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.minStorageBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxVertexBuffers = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxBufferSize = WGPU_LIMIT_U64_UNDEFINED;
	limits.limits.maxVertexAttributes = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxVertexBufferArrayStride = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxInterStageShaderComponents = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxInterStageShaderVariables = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxColorAttachments = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxColorAttachmentBytesPerSample = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxComputeWorkgroupStorageSize = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxComputeInvocationsPerWorkgroup = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxComputeWorkgroupSizeX = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxComputeWorkgroupSizeY = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxComputeWorkgroupSizeZ = WGPU_LIMIT_U32_UNDEFINED;
	limits.limits.maxComputeWorkgroupsPerDimension = WGPU_LIMIT_U32_UNDEFINED;

	limits.limits.maxVertexAttributes = 2;
	limits.limits.maxVertexBuffers = 1;
	limits.limits.maxBufferSize = 6 * 5 * sizeof(float);
	limits.limits.maxVertexBufferArrayStride = 5 * sizeof(float);
	limits.limits.maxInterStageShaderComponents = 3;
	
	return limits;
}

static void request_adapter_cb(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, void *userdata) {
	if (status == WGPURequestAdapterStatus_Success) {
		WGPUAdapter *a = userdata;
		*a = adapter;
	} else {
		fprintf(stderr, "Failed to request adapter: %s\n", message);
		abort();
	}
}

static void request_device_cb(WGPURequestDeviceStatus status, WGPUDevice device, char const *message, void *userdata) {
	if (status == WGPURequestDeviceStatus_Success) {
		WGPUDevice *d = userdata;
		*d = device;
	} else {
		fprintf(stderr, "Failed to request device: %s\n", message);
		abort();
	}
}

static void device_lost_cb(enum WGPUDeviceLostReason reason, const char *message, void *userdata) {
	fprintf(stderr, "Device lost: %d: %s", reason, message);
	abort();
}


static void device_error_cb(WGPUErrorType type, const char *message, void *userdata) {
	fprintf(stderr, "Uncaptured error: %d: %s\n", type, message);
}
