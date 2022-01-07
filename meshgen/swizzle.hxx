#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

inline static glm::ivec2 pack_xn(glm::ivec3 rel) { return {rel.z, rel.y}; }
inline static glm::ivec2 pack_xp(glm::ivec3 rel) { return {rel.y, rel.z}; }
inline static glm::ivec2 pack_yn(glm::ivec3 rel) { return {rel.x, rel.z}; }
inline static glm::ivec2 pack_yp(glm::ivec3 rel) { return {rel.z, rel.x}; }
inline static glm::ivec2 pack_zn(glm::ivec3 rel) { return {rel.y, rel.x}; }
inline static glm::ivec2 pack_zp(glm::ivec3 rel) { return {rel.x, rel.y}; }

inline static glm::ivec3 unpack_xn(glm::ivec2 uv) { return {0, uv.y, uv.x}; }
inline static glm::ivec3 unpack_xp(glm::ivec2 uv) { return {0, uv.x, uv.y}; }
inline static glm::ivec3 unpack_yn(glm::ivec2 uv) { return {uv.x, 0, uv.y}; }
inline static glm::ivec3 unpack_yp(glm::ivec2 uv) { return {uv.y, 0, uv.x}; }
inline static glm::ivec3 unpack_zn(glm::ivec2 uv) { return {uv.y, uv.x, 0}; }
inline static glm::ivec3 unpack_zp(glm::ivec2 uv) { return {uv.x, uv.y, 0}; }
