#pragma once

#include "VulkanContext.h"

#include <map>
#include <string>
#include <vector>

using RenderId = uint32_t;
using BufferId = uint32_t;
using ShaderId = uint32_t;
using PipelineId = uint32_t;
using TextureId = uint32_t;
using SamplerId = uint32_t;
using UniformSetId = uint32_t;

enum class PrimitiveTopology : uint32_t {
	PointList = 0,
	LineList = 1,
	TriangleList = 3
};

struct PipelineAssembly {
	PrimitiveTopology topology;
	bool restart_enable;
};

enum class PolygonMode : uint32_t {
	Fill = 0,
	Line = 1,
	Point = 2
};

enum class CullMode : uint32_t {
	None = 0,
	Front = 1,
	Back = 2,
	FrontAndBack = 3
};

enum class FrontFace : uint32_t {
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

enum class IndexType : uint32_t {
	Uint16 = 0,
	Uint32 = 1
};

enum class UniformType : uint32_t {
	Sampler = 0,
	CombinedImageSampler = 1,
	SampledImage = 2,
	UniformBuffer = 6
};

struct Uniform {
	UniformType type;
	uint32_t binding;
	std::vector<RenderId> ids;
};

enum class Filter : uint32_t {
	Nearest = 0,
	Linear = 1
};

enum class MipMapMode : uint32_t {
	Nearest = 0,
	Linear = 1
};

enum class SamplerAddressMode : uint32_t {
	Repeat = 0,
	MirroredRepeat = 1,
	ClampToEdge = 2,
	ClampToBorder = 3
};

enum class CompareOp {
	Never = 0,
	Less = 1,
	Equal = 2,
	LessOrEqual = 3,
	Greater = 4,
	NotEqual = 5,
	GreaterOrEqual = 6,
	Always = 7
};

enum class BorderColor : uint32_t {
	FloatTransparentBlack = 0,
	IntTransparentBlack = 1,
	FloatOpaqueBlack = 2,
	IntOpaqueBlack = 3,
	FloatOpaqueWhite = 4,
	IntOpaqueWhite = 5
};

struct SamplerInfo {
	Filter mag_filter = Filter::Linear;
	Filter min_filter = Filter::Linear;
	MipMapMode mip_map_mode = MipMapMode::Linear;
	SamplerAddressMode address_mode_u = SamplerAddressMode::Repeat;
	SamplerAddressMode address_mode_v = SamplerAddressMode::Repeat;
	SamplerAddressMode address_mode_w = SamplerAddressMode::Repeat;
	float mip_lod_bias = 0.0f;
	bool anisotropy_enable = false;
	float max_anisotropy = 0.0f;
	bool compare_enable = false;
	CompareOp comapare_op = CompareOp::Always;
	float min_lod = 0.0f;
	float max_lod = 0.0f;
	BorderColor border_color = BorderColor::IntOpaqueBlack;
	bool unnormalized_coordinates = false;
};

class VulkanGraphicsController {
public:
	void create(VulkanContext* context);
	void destroy();

	void resize(uint32_t width, uint32_t height);
	
	void begin_frame();

	void submit(PipelineId pipeline_id, BufferId vertex_id, BufferId index_id, const std::vector<UniformSetId>& uniform_sets);

	void end_frame();

	ShaderId shader_create(const std::vector<uint8_t>& vertex_spv, const std::vector<uint8_t>& fragment_spv);	
	PipelineId pipeline_create(const PipelineInfo* pipeline_info);
	BufferId vertex_buffer_create(const void* data, size_t size);
	BufferId index_buffer_create(const void* data, size_t size, IndexType index_type);
	BufferId uniform_buffer_create(const void* data, size_t size);

	void buffer_update(BufferId buffer_id, const void* data);

	TextureId texture_create(const void* data, uint32_t width, uint32_t height);

	SamplerId sampler_create(const SamplerInfo& info);

	UniformSetId uniform_set_create(ShaderId shader_id, uint32_t set_idx, const std::vector<Uniform>& uniforms);

	//UniformSetId create_uniform_set(const std::vector<Uniform>& uniforms);
	//BufferId create_index_buffer();
	//BufferId create_uniform_buffer();

private:
	// Shader	
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

	// Pipeline
	struct Pipeline {
		PipelineInfo info;
		VkPipeline pipeline;
	};

	// Buffers
	struct VertexBuffer {

	};

	struct IndexBuffer {
		VkIndexType index_type;
		uint32_t index_count;
	};

	struct UniformBuffer {

	};

	struct Buffer {
		VkBuffer buffer;
		VkDeviceSize size;
		VkDeviceMemory memory;
		VkBufferUsageFlags usage;
		union {
			VertexBuffer vertex;
			IndexBuffer index;
			UniformBuffer uniform;
		};
	};

	VkBuffer buffer_create(VkBufferUsageFlags usage, VkDeviceSize size);
	VkDeviceMemory buffer_allocate(VkBuffer buffer, VkMemoryPropertyFlags mem_props);
	void buffer_copy(VkBuffer buffer, const void* data, VkDeviceSize size);
	
	// Images
	struct Image {
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
		VkExtent2D extent;
		VkImageUsageFlags usage;
		VkImageLayout layout;
	};

	VkImage image_create(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage);
	VkDeviceMemory image_allocate(VkImage image, VkMemoryPropertyFlags mem_props);
	VkImageView image_view_create(VkImage image, VkFormat format, VkImageAspectFlags aspect);
	void image_copy(VkImage image, VkExtent2D extent, VkImageAspectFlags aspect, VkImageLayout layout, const void* data, size_t size);
	void transition_image_layout(VkImage image, VkFormat, VkImageAspectFlags aspect, VkImageLayout old_layout, VkImageLayout new_layout);

	uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	// Sampler
	struct Sampler {
		SamplerInfo info;
		VkSampler sampler;
	};

	// Descriptor Pool
	struct DescriptorPoolKey {
		uint16_t uniform_type_counts[10];

		bool operator<(const DescriptorPoolKey& other) const {
			return 0 > memcmp(uniform_type_counts, other.uniform_type_counts, sizeof(uniform_type_counts));
		}

		DescriptorPoolKey() {
			memset(uniform_type_counts, 0, sizeof(uniform_type_counts));
		}
	};

	static constexpr uint32_t MAX_SETS_PER_DESCRIPTOR_POOL = 64;

	struct DescriptorPool {
		VkDescriptorPool pool;
		uint32_t usage_count;
	};

	uint32_t descriptor_pool_allocate(const DescriptorPoolKey& key);
	void descriptor_pools_free();

	// Uniform Set
	struct UniformSet {
		DescriptorPoolKey pool_key;
		uint32_t pool_idx;
		ShaderId shader;
		uint32_t set_idx;
		VkDescriptorSet descriptor_set;
	};

	// Draw Call
	struct DrawCall {
		PipelineId pipeline;
		BufferId vertex_buffer;
		BufferId index_buffer;
		std::vector<UniformSetId> uniform_sets;
	};

	void find_depth_format();
	void create_depth_resources();
	void destroy_depth_resources();
	void create_render_pass();
	void create_framebuffer();
	void destroy_framebuffer();

private:
	VulkanContext* m_context;

	std::vector<Shader> m_shaders;
	std::vector<Pipeline> m_pipelines;
	std::vector<Buffer> m_buffers;
	std::vector<Image> m_images;
	std::vector<Sampler> m_samplers;
	std::map<DescriptorPoolKey, std::vector<DescriptorPool>> m_descriptor_pools;
	std::vector<UniformSet> m_uniform_sets;

	std::vector<DrawCall> m_draw_calls;

	// TODO: Delete these lines
	VkFormat m_depth_format;
	std::vector<Image> m_depth_images;
	VkRenderPass m_default_render_pass;
	std::vector<VkFramebuffer> m_default_framebuffers;
	// Delete down here
};