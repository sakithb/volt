@fragment
fn main(@location(0) vPos : vec2<f32>) -> @location(0) vec4<f32> {
	return vec4<f32>(vPos, 1.0, 1.0);
}
