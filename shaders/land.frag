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
	vec3 color = texture(tex, vec3(v_uv, layer)).xyz * brightness * brightness;
	float lum = dot(vec3(0.2126, 0.7152, 0.0722), color);
	float coef_sat = exp(-distance * 0.001);
	float coef_lum = exp(-distance * 0.0001);
	color = mix(vec3(0.5, 0.7, 1.0), mix(vec3(lum), color, coef_sat), coef_lum);
	gl_FragColor = vec4(color, 1.0);
}
