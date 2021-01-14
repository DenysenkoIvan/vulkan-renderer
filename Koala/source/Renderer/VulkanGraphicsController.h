#pragma once

#include "VulkanContext.h"

#include <string>

using RenderId = uint32_t;
using BufferId = uint32_t;
using ShaderId = uint32_t;
using PipelineId = uint32_t;
using UniformSetId = uint32_t;

enum class PrimitiveTopology {
	PointList = 0,
	LineList = 1,
	TriangleList = 3
};

struct PipelineAssembly {
	PrimitiveTopology topology;
	bool restart_enable;
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

struct Rasterization {
	bool depth_clamp_enable;
	bool rasterizer_discard_enable;
	PolygonMode polygon_mode;
	CullMode cull_mode;
	FrontFace front_face;
	bool depth_bias_enable;
	float depth_bias_constant_factor;
	float depth_bias_clamp;
	float detpth_bias_slope_factor;
	float line_width;
};

struct PipelineInfo {
	ShaderId shader;
	PipelineAssembly assembly;
	Rasterization raster;
};

enum class UniformType : uint32_t {
	Sampler = 0,
	CombinedImageSampler = 1,
	SampledImage = 2,
	UniformBuffer = 6
};

struct UniformInfo {
	UniformType type;
	uint32_t binding;
	RenderId id;
};

enum class IndexType {
	Uint16 = 0,
	Uint32 = 1
};

class VulkanGraphicsController {
public:
	void create(VulkanContext* context);
	void destroy();

	void resize(uint32_t width, uint32_t height);
	
	void begin_frame();
	void end_frame();

	//void submit_geometry(BufferId vertex_buffer, BufferId index_buffer);


	ShaderId shader_create(const std::vector<uint8_t>& vertex_spv, const std::vector<uint8_t>& fragment_spv);
	
	PipelineId pipeline_create(const PipelineInfo* pipeline_info);

	BufferId vertex_buffer_create(const void* data, size_t size);
	BufferId index_buffer_create(const void* data, size_t size, IndexType index_type);
	//UniformSetId create_uniform_set(const std::vector<Uniform>& uniforms);
	//BufferId create_index_buffer();
	//BufferId create_uniform_buffer();

private:
	VkBuffer buffer_create(VkBufferUsageFlags usage, VkDeviceSize size);
	VkDeviceMemory memory_buffer_allocate(VkBuffer buffer, VkMemoryPropertyFlags mem_props);

	void buffer_copy(const void* data, VkDeviceSize size, VkBuffer buffer);
	
	uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	void create_framebuffer();
	void destroy_framebuffer();

private:
	struct Set {
		uint32_t set;
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		std::vector<VkDescriptorSetLayoutBinding>::iterator find_binding(uint32_t binding_idx) {
			return std::find_if(bindings.begin(), bindings.end(), [binding_idx](const auto& binding) { return binding.binding == binding_idx; });
		}
	};

	struct ShaderInfo {
		std::vector<Set> sets;
		std::vector<VkDescriptorSetLayout> set_layouts;
		std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
		VkVertexInputBindingDescription binding_description;
		std::string vertex_entry;
		std::string fragment_entry;
		VkShaderModule vertex_module;
		VkShaderModule fragment_module;

		std::vector<Set>::iterator find_set(uint32_t set_id) {
			return std::find_if(sets.begin(), sets.end(), [set_id](const Set& set) { return set.set == set_id; });
		}
	};

	struct Shader {
		std::unique_ptr<ShaderInfo> info;

		std::array<VkPipelineShaderStageCreateInfo, 2> stage_create_infos;
		VkPipelineVertexInputStateCreateInfo vertex_input_create_info;
		VkPipelineLayout pipeline_layout;
	};

	struct Pipeline {
		PipelineInfo info;
		VkPipeline pipeline;
	};

	struct VertexBuffer {

	};

	struct IndexBuffer {
		VkIndexType index_type;
	};

	struct Buffer {
		VkBuffer buffer;
		VkDeviceSize size;
		VkDeviceMemory memory;
		VkBufferUsageFlags usage;
		union {
			VertexBuffer vertex;
			IndexBuffer index;
		};
	};

	VulkanContext* m_context;

	std::vector<Shader> m_shaders;
	std::vector<Pipeline> m_pipelines;
	std::vector<Buffer> m_buffers;

	// TODO: Delete these lines
	VkRenderPass m_default_render_pass;
	std::vector<VkFramebuffer> m_default_framebuffers;
	// Delete down here
};