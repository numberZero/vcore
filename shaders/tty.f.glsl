#version 430

layout(location = 0) uniform vec4 color = vec4(0.75, 0.75, 0.75, 1.00);
layout(binding = 0) uniform usampler2D tty;
layout(binding = 1) uniform sampler2DArray font;
in vec2 cc;
layout(location = 0) out vec4 result;

void main()
{
	ivec2 char_pos = ivec2(cc);
	vec2 char_off = cc - char_pos;
	uint char = texelFetch(tty, char_pos, 0).r;
	float light = texture(font, vec3(char_off, char)).r;
	result = light * color;
}
