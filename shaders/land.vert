#version 330

uniform mat4 m;

in vec3 position;
in vec3 color;
out vec3 pos;
out vec3 v_color;
out float distance;

void main() {
	pos = position;
	v_color = color;
	gl_Position = m * vec4(position, 1.0);
	distance = gl_Position.w;
}
