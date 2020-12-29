#pragma once

#include <cstdint>

enum class IndexType {
	Uint16 = 0,
	Uint32 = 1
};

enum class Format {
	Undefined = 0,
	R32_UInt = 98,
	R32_SInt = 99,
	R32_SFloat = 100,
	RG32_Uint = 101,
	RG32_SInt = 102,
	RG32_SFloat = 103,
	RGB32_UInt = 104,
	RGB32_SInt = 105,
	RGB32_SFloat = 106,
	RGBA32_UInt = 107,
	RGBA32_SInt = 108,
	RGBA32_SFloat = 109
};

struct Offset {
	int32_t x;
	int32_t y;
};

struct Extent2D {
	uint32_t width;
	uint32_t height;
};

struct Rect2D {
	Offset offset;
	Extent2D extent;
};

enum class CompareOp {
	Never = 0,
	Less = 1,
	Equal = 2,
	LessOrEqual = 3,
	Greater = 4,
	NotEqual = 5,
	GreateOrEqual = 6,
	Always = 7
};

enum class StencilOp {
	Keep = 0,
	Zero = 1,
	Replace = 2,
	IncrementAndClamp = 3,
	DecrementAndClamp = 4,
	Invert = 5,
	IncrementAndWrap = 6,
	DecrementAndWrap = 7
};

enum class LogicOp {
	Clear = 0,
	And = 1,
	AndReverse = 2,
	Copy = 3,
	AndInverted = 4,
	NoOp = 5,
	Xor = 6,
	Or = 7,
	Nor = 8,
	Equivalent = 9,
	Invert = 10,
	OrReverse = 11,
	CopyInverted = 12,
	OrInverted = 13,
	Nand = 14,
	Set = 15
};

enum class VertexInputRate {
	Vertex = 0,
	Instance = 1
};