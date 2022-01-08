#!/usr/bin/python3

from sys import argv, stdin, stdout, stderr
from PIL import Image

name = argv[1]
nx = int(argv[2])
ny = int(argv[3])
base = None
if len(argv) > 4:
	base = int(argv[4])

with Image.open("%s.png" % name) as img:
	width, height = img.size
	data = img.tobytes()
assert(len(data) == 3 * width * height)
assert(width % nx == 0)
assert(height % ny == 0)

glyph_width = width // nx
glyph_height = height // ny
glyph_count = nx * ny

glyph = 0
print("// Automatically generated - do not edit!")
print("#include \"rasterfont.h\"")
print()
print("extern const struct RasterFont %s;" % name)
print("const struct RasterFont %s = {" % name)
print("\t.glyph_width = %d," % glyph_width)
print("\t.glyph_height = %d," % glyph_height)
print("\t.glyph_count = %d," % glyph_count)
if base != None:
	print("\t.glyph_base = %d," % base)
print("\t.data = {")

for j in range(ny):
	for i in range(nx):
		print("\t\t", end='')
		for v in range(glyph_height):
			for u in range(glyph_width):
				print(data[3 * (glyph_width * i + u + (glyph_height * j + v) * width)], end=',')
			print(end=' ')
		if base != None:
			print(" // %d %s" % (glyph, repr(chr(base + glyph))))
		else:
			print(" // %d" % glyph)
		glyph += 1

print("}};")
