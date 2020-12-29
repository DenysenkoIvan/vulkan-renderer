#pragma once

#include "Common.h"
#include "Shader.h"

#include <memory>
#include <vector>

struct VertexInputBinding {
	uint32_t binding;
	uint32_t stride;
	VertexInputRate input_rate;
};

struct VertexInputAttribute {
	uint32_t location;
	uint32_t binding;
	Format format;
	uint32_t offset;
};

struct VertexInputState {
	std::vector<VertexInputBinding> input_bindings;
	std::vector<VertexInputAttribute> input_attributes;
};

enum class PrimitiveTopology {
	PointList = 0,
	LintList = 1,
	LineStrip = 2,
	TriangleList = 3,
	TriangleStrip = 4,
	TriangleFan = 5,
	LineListWithAdjacency = 6,
	LineStripWithAdjacency = 7,
	TriangleListWithAdjacency = 8,
	TriangleStripWithAdjacency = 9,
	PatchList = 10
};

struct InputAssemblyState {
	PrimitiveTopology topology;
	bool primitive_restart_enable;
};

struct TesselationState {
	uint32_t patch_control_points;
};

struct Viewport {
	float x;
	float y;
	float width;
	float height;
	float min_depth;
	float max_depth;
};

struct ViewportState {
	std::vector<Viewport> viewports;
	std::vector<Rect2D> scissors;
};

enum class PolygonMode {
	Fill = 0,
	Line = 1,
	Point = 2
};

enum class CullMode {
	None = 0,
	Front = 1,
	Back = 2,
	FrontAndBack = 3
};

enum class FrontFace {
	CounterClockwise = 0,
	Clockwise = 1
};

struct RasterizationState {
	bool depth_clamp_enable;
	bool rasterizer_discard_enable;
	PolygonMode polygon_mode;
	CullMode cull_mode;
	FrontFace front_face;
	bool depth_bias_enable;
	float depth_bias_constant_factor;
	float depth_bias_clamp;
	float depth_bias_slope_factor;
	float line_width;
};

enum class SampleCount {
	One = 1,
	Two = 2,
	Four = 4,
	Eight = 8,
	Sixteen = 16,
	ThirtyTwo = 32,
	SixtyFour = 64
};

struct MultisampleState {
	SampleCount sample_count;
	bool sample_shading_enable;
	float min_sample_shading;
	std::vector<uint32_t> sample_mask;
	bool alpha_to_coverage_enable;
	bool alpha_to_one_enable;
};

struct StencilOpState {
	StencilOp fail_op;
	StencilOp pass_op;
	StencilOp depth_fail_op;
	CompareOp compare_op;
	uint32_t compare_mask;
	uint32_t wirte_mask;
	uint32_t reference;
};

struct DepthStencilState {
	bool depth_test_enable;
	bool depth_write_enable;
	CompareOp depth_compare_op;
	bool depth_bounds_test_enable;
	bool stencil_test_enable;
	StencilOpState front;
	StencilOpState back;
	float min_depth_bounds;
	float max_depth_bounds;
};

struct ColorBlendState {
	bool logic_op_enable;
	LogicOp logic_op;

};

struct PipelineInfo {
	//std::shared_ptr<Shader> shader;
	VertexInputState vertex_input;
	InputAssemblyState input_assembly;
	TesselationState tesselation;
	ViewportState viewport;
	RasterizationState rasterization;
	MultisampleState multisample;
	DepthStencilState depth_stencil;
	ColorBlendState color_blend;
};

class Pipeline {
public:

	static std::shared_ptr<Pipeline> create();

private:
	Pipeline() = default;

private:
	//std::shared_ptr<Shader> m_shader;
	//VkPipeline m_pipeline;
};