#pragma once

#include <cstdint>

using RenderId = size_t;

enum class Format : uint32_t {
	Undefined = 0,
	RGBA8_UNorm = 37,
	RGBA8_SNorm = 38,
	RGBA8_SRGB = 43,
	BGRA8_UNorm = 44,
	//BGRA8_SNorm = 45,
	RGBA16_UNorm = 90,
	RGBA16_SFloat = 97,
	R32_UInt = 98,
	R32_SInt = 99,
	R32_SFloat = 100,
	RG32_UInt = 101,
	RG32_SInt = 102,
	RG32_SFloat = 103,
	RGB32_UInt = 104,
	RGB32_SInt = 105,
	RGB32_SFloat = 106,
	RGBA32_UInt = 107,
	RGBA32_SInt = 108,
	RGBA32_SFloat = 109,
	D32_SFloat = 126,
	D24_UNorm_S8_UInt = 129,
	D32_SFloat_S8_UInt = 130
};