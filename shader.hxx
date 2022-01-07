#pragma once
#include <vector>
#include <gl++/c.hxx>

using bytearray = std::vector<std::byte>;
class gl_exception: public std::runtime_error { using runtime_error::runtime_error; };

unsigned compile_shader(gl::GLenum type, bytearray const &source);
unsigned link_program(std::vector<unsigned> shaders, bool delete_shaders = true);
