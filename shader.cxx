#include "shader.hxx"
#include <cassert>
#include <fmt/printf.h>

using namespace gl;

unsigned compile_shader(GLenum type, bytearray const &source) {
	unsigned shader = fn.CreateShader(type);
	if (!shader)
		throw gl_exception(fmt::sprintf("Can't create shader (of type %#x)", type));
	char const *source_data = reinterpret_cast<char const *>(source.data());
	int source_length = source.size();
	fn.ShaderSource(shader, 1, &source_data, &source_length);
	fn.CompileShader(shader);
	int compiled = 0;
	fn.GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled)
		return shader;
	int log_length = 0;
	fn.GetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
	if (!log_length)
		throw gl_exception(fmt::sprintf("Can't compile shader (of type %#x); no further information available.", type));
	std::vector<char> log(log_length);
	fn.GetShaderInfoLog(shader, log.size(), &log_length, log.data());
	assert(log.size() == log_length + 1);
	throw gl_exception(fmt::sprintf("Can't compile shader (of type %#x): %s", type, log.data()));
}

unsigned link_program(std::vector<unsigned> shaders, bool delete_shaders) {
	unsigned program = fn.CreateProgram();
	if (!program)
		throw gl_exception("Can't create program");
	for (GLenum shader: shaders)
		fn.AttachShader(program, shader);
	fn.LinkProgram(program);
	for (GLenum shader: shaders)
		fn.DetachShader(program, shader);
	int linked = 0;
	fn.GetProgramiv(program, GL_LINK_STATUS, &linked );
	if (linked) {
		if (delete_shaders)
			for (GLenum shader: shaders)
				fn.DeleteShader(shader);
		return program;
	}
	int log_length = 0;
	fn.GetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
	if (!log_length)
		throw gl_exception("Can't link program; no further information available.");
	std::vector<char> log(log_length);
	fn.GetProgramInfoLog(program, log.size(), &log_length, log.data());
	assert(log.size() == log_length + 1);
	throw gl_exception(fmt::sprintf("Can't link program: %s", log.data()));
}
