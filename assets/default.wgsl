struct VertexOutput {
	@builtin(position) position: vec4<f32>,
	@location(0) color: vec4<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOutput {
    var p = vec2<f32>(0.0f);
	var c = vec3<f32>(0.0f);

    if in_vertex_index == 0u {
        p = vec2<f32>(-0.5, -0.5);
		c = vec3<f32>(1.0f, 0.0f, 0.0f);
    } else if in_vertex_index == 1u {
        p = vec2<f32>(0.5, -0.5);
		c = vec3<f32>(0.0f, 1.0f, 0.0f);
    } else {
        p = vec2<f32>(0.0, 0.5);
		c = vec3<f32>(0.0f, 0.0f, 1.0f);
    }

	var out: VertexOutput;
	out.position = vec4<f32>(p, 0.0, 1.0);
	out.color = vec4<f32>(c, 1.0f);

    return out;
}

@fragment
fn fs_main(@location(0) color: vec4<f32>) -> @location(0) vec4<f32> {
	return color;
}

