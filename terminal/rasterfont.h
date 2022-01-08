#pragma once

struct RasterFont {
	int glyph_width;
	int glyph_height;
	int glyph_count;
	int glyph_base;
	unsigned char data[];
};
