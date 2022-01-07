#version 330

uniform mat4 m;

in vec3 position;
in vec4 color;
in vec2 uv;
in uint type;
out vec3 pos;
out vec3 v_color;
out float brightness;
out vec2 v_uv;
flat out uint layer;
out float distance;

void main() {
	pos = position;
	v_color = color.rgb;
	brightness = color.a;
	v_uv = uv;
	layer = type;
	gl_Position = m * vec4(position, 1.0);
	distance = gl_Position.w;
}
