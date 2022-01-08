#version 430

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 1) uniform uvec2 size;
out vec2 cc;

void main() {
	cc = vec2(size.x, size.y);
	gl_Position = vec4(1.0, -1.0, 0.0, 1.0);
	EmitVertex();

	cc = vec2(size.x, 0.0);
	gl_Position = vec4(1.0, 1.0, 0.0, 1.0);
	EmitVertex();

	cc = vec2(0.0, size.y);
	gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);
	EmitVertex();

	cc = vec2(0.0, 0.0);
	gl_Position = vec4(-1.0, 1.0, 0.0, 1.0);
	EmitVertex();
}
