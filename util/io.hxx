#pragma once
#include <string>
#include <vector>

using bytearray = std::vector<std::byte>;

bytearray read_file(std::string const &filename);
