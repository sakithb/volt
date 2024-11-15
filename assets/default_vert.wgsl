struct VertexIn {
	@location(0) aPos: vec2<f32>,
}

struct VertexOut {
	@location(0) vPos: vec2<f32>,
	@builtin(position) Position: vec4<f32>
}

@vertex
fn main(input: VertexIn) -> VertexOut {
	output.Position = vec4<f32>(input.aPos, 1.0, 1.0);
	return input.aPos;
}
