#pragma once
#include <cassert>
#include <stdexcept>
#include <fmt/format.h>
#include <glm/vec2.hpp>

class TTY {
public:
	static constexpr int width = 80;
	static constexpr int height = 25;
	static constexpr int tab_width = 8;
	static constexpr int buf_size = width * height;
	char data[buf_size];
	bool capitalize = true;

	TTY() {
		clear();
	}

	glm::ivec2 cursor() const noexcept {
		assert(cursor_pos < buf_size);
		return {cursor_pos % width, cursor_pos / width};
	}

	void cursor(glm::ivec2 pos) {
		if (pos.x < 0 || pos.y < 0)
			throw std::out_of_range("Negative TTY cursor position");
		if (pos.x >= width || pos.y >= height)
			throw std::out_of_range("Cursor position out of TTY");
		cursor_pos = pos.x + width * pos.y;
	}

	void clear(char c = ' ') {
		cursor_pos = 0;
		std::memset(data, c, buf_size);
	}

	void raw_put(char c) {
		assert(cursor_pos < buf_size);
		data[cursor_pos++] = c;
		if (cursor_pos == buf_size) {
			cursor_pos -= width;
			std::memmove(data, data + width, buf_size - width);
		}
	}

	void println() {
		while (cursor_pos % width)
			raw_put(' ');
	}

	void put(char c) {
		switch (c) {
			case '\t':
				while (cursor_pos % width % tab_width)
					raw_put(' ');
				break;
			case '\n':
				println();
				break;
			case '\r':
				break;
			case '\x20'...'\x7e':
				if (capitalize)
					raw_put(std::toupper(c));
				else
					raw_put(c);
				break;
			case '\x7f':
				if (cursor_pos % width)
					cursor_pos--;
				break;
			default:
				raw_put('\x1f');
				break;
		}
	}

	void print(std::string_view s) {
		for (char c: s)
			put(c);
	}

	void println(std::string_view s) {
		print(s);
		println();
	}

	template <typename ...Args>
	void println(std::string_view format, Args &&...args) {
		print(format, std::forward<Args>(args)...);
		println();
	}

	template <typename ...Args>
	void print(std::string_view format, Args &&...args) {
		print(fmt::format(format, std::forward<Args>(args)...));
	}

private:
	int cursor_pos;
};
