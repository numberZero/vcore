#version 330

in vec3 pos;
in vec3 v_color;
void main() {
// 	gl_FragColor = vec4(pos.z / 16.0, 1.0, 0.0, 1.0);
	gl_FragColor = vec4(v_color, 1.0);
}
