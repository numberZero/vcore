#pragma once
#include <string>
#include <vector>

typedef unsigned char byte;
typedef std::vector<byte> bytearray;

bytearray read_file(std::string const &filename);
