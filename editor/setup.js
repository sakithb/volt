/** @type {EmscriptenModule & { preinitializedWebGPUDevice: GPUDevice } } */
var Module = {};

Module.onRuntimeInitialized = async () => {
	if (navigator.gpu) {
		try {
			const adapter = await navigator.gpu.requestAdapter();
			const device = await adapter.requestDevice();

			Module.preinitializedWebGPUDevice = device;
			Module.ccall("setup", "undefined", [], []);
		} catch (error) {
			throw new Error("Failed to initialize WebGPU: " + error.toString());
		}
	} else {
		throw new Error("Failed to initialize WebGPU: not supported!");
	}
}
