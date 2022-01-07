#version 330

in vec3 pos;
in vec3 v_color;
in float distance;

void main() {
	float lum = dot(vec3(0.2126, 0.7152, 0.0722), v_color);
	float coef = exp(-distance * 0.005);
	vec3 color = mix(vec3(lum), v_color, clamp(coef, 0.0, 1.0));
	gl_FragColor = vec4(color, 1.0);
}
