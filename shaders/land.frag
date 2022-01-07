#version 330

uniform sampler2DArray tex;

in vec3 pos;
in vec3 v_color;
in vec2 v_uv;
in float brightness;
flat in uint layer;
in float distance;

void main() {
// 	vec3 color = v_color;
	vec3 color = texture(tex, vec3(v_uv, layer)).xyz * brightness;
	float lum = dot(vec3(0.2126, 0.7152, 0.0722), color);
	float coef = exp(-distance * 0.005);
	color = mix(vec3(lum), color, clamp(coef, 0.0, 1.0));
	gl_FragColor = vec4(color, 1.0);
}
