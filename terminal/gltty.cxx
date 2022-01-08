#include "gltty.hxx"
#include <filesystem>
#include <gl++/c.hxx>
#include <glm/gtc/type_ptr.hpp>
#include "util/io.hxx"
#include "shader.hxx"
#include "rasterfont.h"

extern const RasterFont font_ascii;

namespace fs = std::filesystem;
extern fs::path app_root;

using namespace gl;

static unsigned loadRasterFont(RasterFont const *font) {
	unsigned tex;
	fn.GenTextures(1, &tex);
	fn.BindTexture(GL_TEXTURE_2D_ARRAY, tex);
	fn.TexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R8, font->glyph_width, font->glyph_height, font->glyph_count);
	fn.TexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, font->glyph_width, font->glyph_height, font->glyph_count, GL_RED, GL_UNSIGNED_BYTE, font->data);
	fn.TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	fn.TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	fn.TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	fn.TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return tex;
}

void GLTTY::init() {
	prog = link_program({
		compile_shader(GL_VERTEX_SHADER, read_file(app_root / "shaders/empty.v.glsl")),
		compile_shader(GL_GEOMETRY_SHADER, read_file(app_root / "shaders/tty.g.glsl")),
		compile_shader(GL_FRAGMENT_SHADER, read_file(app_root / "shaders/tty.f.glsl")),
	});

	font = loadRasterFont(&font_ascii);

	fn.CreateTextures(GL_TEXTURE_2D, 1, &data_texture);
	fn.TextureStorage2D(data_texture, 1, GL_R8UI, width, height);
	fn.TextureParameteri(data_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	fn.TextureParameteri(data_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void GLTTY::render() {
	fn.TextureSubImage2D(data_texture, 0, 0, 0, width, height, GL_RED_INTEGER, GL_UNSIGNED_BYTE, data);
	fn.UseProgram(prog);
	fn.Uniform4fv(0, 1, glm::value_ptr(color));
	fn.Uniform2ui(1, width, height);
	fn.BindTextureUnit(0, data_texture);
	fn.BindTextureUnit(1, font);
	fn.BindVertexArray(0);
	fn.DrawArrays(GL_POINTS, 0, 1);
}
