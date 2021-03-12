#pragma once

#include "VulkanContext.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

using RenderId = uint32_t;
using RenderPassId = uint32_t;
using FramebufferId = uint32_t;
using ImageId = uint32_t;
using BufferId = uint32_t;
using ShaderId = uint32_t;
using PipelineId = uint32_t;
using SamplerId = uint32_t;
using UniformSetId = uint32_t;

enum class Format {
	Undefined = 0,
	RGBA8_SRGB = 43,
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

enum ImageUsageFlagBits {
	ImageUsageTransferSrc = 1,
	ImageUsageTransferDst = 2,
	ImageUsageSampled = 4,
	ImageUsageColorAttachment = 16,
	ImageUsageDepthStencilAttachment = 32,
	ImageUsageCPUVisible = 1048576
};
using ImageUsageFlags = uint32_t;

enum class ImageViewType {
	OneD = 0,
	TwoD = 1,
	ThreeD = 2,
	Cube = 3
};

enum class InitialAction {
	Load = 0,
	Clear = 1,
	DontCare = 2
};

enum class FinalAction {
	Store = 0,
	DontCare = 1
};

struct RenderPassAttachment {
	ImageUsageFlags usage;
	Format format = Format::Undefined;
	InitialAction initial_action;
	FinalAction final_action;
};

enum class PrimitiveTopology : uint32_t {
	PointList = 0,
	LineList = 1,
	TriangleList = 3
};

struct PipelineAssembly {
	PrimitiveTopology topology = PrimitiveTopology::TriangleList;
	bool restart_enable = false;
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
	bool depth_clamp_enable = false;
	bool rasterizer_discard_enable = false;
	PolygonMode polygon_mode = PolygonMode::Fill;
	CullMode cull_mode = CullMode::None;
	FrontFace front_face;
	bool depth_bias_enable = false;
	float depth_bias_constant_factor;
	float depth_bias_clamp;
	float detpth_bias_slope_factor;
	float line_width = 1.0f;
};

enum PipelineDynamicState {
	DYNAMIC_STATE_VIEWPORT = 0,
	DYNAMIC_STATE_SCISSOR = 1,
	DYNAMIC_STATE_LINE_WIDTH = 2,
	DYNAMIC_STATE_DEPTH_BIAS = 3,
	DYNAMIC_STATE_DEPTH_BOUNDS = 5
};
using PipelineDynamicStateFlags = uint32_t;

struct DynamicStates {
	PipelineDynamicStateFlags* dynamic_states = nullptr;
	uint32_t dynamic_state_count = 0;
};

struct PipelineInfo {
	ShaderId shader_id;
	PipelineAssembly assembly;
	Rasterization raster;
	DynamicStates dynamic_states;
	std::optional<RenderPassId> render_pass_id;
};

struct ImageInfo {
	ImageUsageFlags usage;
	ImageViewType view_type = ImageViewType::TwoD;
	Format format;
	uint32_t width;
	uint32_t height;
	uint32_t depth = 1;
	uint32_t layer_count = 1;
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
	const RenderId* ids;
	uint32_t id_count;
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
	SamplerAddressMode address_mode_u = SamplerAddressMode::ClampToEdge;
	SamplerAddressMode address_mode_v = SamplerAddressMode::ClampToEdge;
	SamplerAddressMode address_mode_w = SamplerAddressMode::ClampToEdge;
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

	void end_frame();
	
	void draw_begin(FramebufferId framebuffer_id, const glm::vec4* clear_colors, uint32_t count);
	void draw_end();

	void draw_begin_for_screen(const glm::vec4& clear_color);
	void draw_end_for_screen();

	void draw_set_viewport(float x, float y, float width, float height, float min_depth, float max_depth);
	void draw_set_scissor(int x_offset, int y_offset, uint32_t width, uint32_t height);

	void draw_bind_pipeline(PipelineId pipeline_id);
	void draw_bind_vertex_buffer(BufferId buffer_id);
	void draw_bind_index_buffer(BufferId buffer_id, IndexType index_type);
	void draw_bind_uniform_sets(PipelineId pipeline_id, uint32_t first_set, const UniformSetId* set_ids, uint32_t count);
	void draw_draw_indexed(uint32_t index_count);

	RenderPassId render_pass_create(const RenderPassAttachment* attachments, uint32_t count);

	FramebufferId framebuffer_create(RenderPassId render_pass_id, const ImageId* ids, uint32_t count);

	ShaderId shader_create(const void* vertex_spv, uint32_t vert_size, const void* fragment_spv, uint32_t frag_size);
	
	PipelineId pipeline_create(const PipelineInfo& pipeline_info);
	
	BufferId vertex_buffer_create(const void* data, size_t size);
	BufferId index_buffer_create(const void* data, size_t size, IndexType index_type);
	BufferId uniform_buffer_create(const void* data, size_t size);

	void buffer_update(BufferId buffer_id, const void* data);

	ImageId image_create(const void* data, const ImageInfo& info);
	void image_update(ImageId image_id, const void* data, size_t size);

	SamplerId sampler_create(const SamplerInfo& info);

	UniformSetId uniform_set_create(ShaderId shader_id, uint32_t set_idx, const Uniform* uniforms, uint32_t uniform_count);

private:
	struct RenderPassAttachmentInfo {
		RenderPassAttachment attachment;
		VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout final_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	};

	struct RenderPass {
		std::vector<RenderPassAttachmentInfo> attachments;
		VkRenderPass render_pass;
	};

	std::vector<RenderPass> m_render_passes;

	struct Framebuffer {
		std::vector<ImageId> attachments;
		RenderPassId render_pass_id;
		VkRenderPass render_pass;
		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		VkExtent2D extent;
	};

	std::vector<Framebuffer> m_framebuffers;

	// Shader	
	struct SetInfo {
		uint32_t set;
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		std::vector<VkDescriptorSetLayoutBinding>::iterator find_binding(uint32_t binding_idx) {
			return std::find_if(bindings.begin(), bindings.end(), [binding_idx](const auto& binding) {
				return binding.binding == binding_idx;
			});
		};
	};

	struct StageInfo {
		std::vector<char> entry;
		VkShaderModule module;
	};

	struct InputVarsInfo {
		std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
		VkVertexInputBindingDescription binding_description;
	};

	struct Shader {
		std::vector<SetInfo> sets;
		std::vector<StageInfo> stages;
		InputVarsInfo input_vars_info;

		std::vector<VkDescriptorSetLayout> set_layouts;
		std::vector<VkPipelineShaderStageCreateInfo> stage_create_infos;
		VkPipelineVertexInputStateCreateInfo vertex_input_create_info;
		VkPipelineLayout pipeline_layout;

		std::vector<SetInfo>::iterator find_set(uint32_t set_idx) {
			return std::find_if(sets.begin(), sets.end(), [set_idx](const auto& set) {
				return set.set == set_idx;
			});
		}
	};

	// Pipeline
	struct Pipeline {
		PipelineInfo info;
		VkPipelineLayout layout; // Not owned
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
	void buffer_memory_barrier(VkBuffer& buffer, VkBufferUsageFlags usage, VkDeviceSize offset, VkDeviceSize size);

	// Images
	struct Image {
		ImageInfo info;
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
		VkImageLayout current_layout;
		VkImageAspectFlags aspect;
	};

	VkImage vulkan_image_create(ImageViewType view_type, VkFormat format, VkExtent3D extent, uint32_t layer_count, VkImageTiling tiling, VkImageUsageFlags usage);
	VkDeviceMemory vulkan_image_allocate(VkImage image, VkMemoryPropertyFlags mem_props);
	VkImageView vulkan_image_view_create(VkImage image, VkImageViewType view_type, VkFormat format, VkImageAspectFlags aspect, uint32_t layer_count);
	void vulkan_image_copy(VkImage image, VkFormat format, VkExtent3D extent, VkImageAspectFlags aspect, VkImageLayout layout, uint32_t layer_count, const void* data, size_t size);
	void image_should_have_layout(Image& image, VkImageLayout layout);
	void image_layout_transition(Image& image, VkImageLayout new_layout);
	void vulkan_image_memory_barrier(VkImage image, VkImageAspectFlags aspect, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t layer_count);

	uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);

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
		std::vector<ImageId> images; // Used to check out if image is in proper layout before descriptor binding operation
		DescriptorPoolKey pool_key;
		uint32_t pool_idx;
		ShaderId shader;
		uint32_t set_idx;
		VkDescriptorSet descriptor_set;
	};

private:
	struct StagingBuffer {
		VkBuffer buffer;
		VkDeviceMemory memory;
	};

	struct Frame {
		VkCommandPool command_pool;
		VkCommandBuffer setup_buffer;
		VkCommandBuffer draw_buffer;
		std::vector<StagingBuffer> staging_buffers;
	};

	VulkanContext* m_context;
	std::vector<Frame> m_frames;
	uint32_t m_frame_index;

	std::vector<Shader> m_shaders;
	std::vector<Pipeline> m_pipelines;
	std::vector<Buffer> m_buffers;
	std::vector<Image> m_images;
	std::vector<Sampler> m_samplers;
	std::map<DescriptorPoolKey, std::vector<DescriptorPool>> m_descriptor_pools;
	std::vector<UniformSet> m_uniform_sets;
};