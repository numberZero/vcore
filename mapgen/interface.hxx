#pragma once
#include "map/block.hxx"

class IMapgen {
public:
	virtual ~IMapgen() = default;
	virtual Block generate(BlockPosition block_pos) = 0;
};
