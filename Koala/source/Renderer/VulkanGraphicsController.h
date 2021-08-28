#pragma once

#include "Common.h"
#include "VulkanContext.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

using RenderPassId = RenderId;
using FramebufferId = RenderId;
using ImageId = RenderId;
using BufferId = RenderId;
using ShaderId = RenderId;
using PipelineId = RenderId;
using SamplerId = RenderId;
using UniformSetId = RenderId;

enum ImageUsageFlagBits {
	ImageUsageNone = 0,
	ImageUsageTransferSrc = 1,
	ImageUsageTransferDst = 2,
	//ImageUsageSampled = 4,
	ImageUsageColorSampled = 0x100000,
	ImageUsageDepthSampled = 0x200000,
	//ImageUsageStencliSampled = 0x400000,
	ImageUsageColorAttachment = 0x10,
	ImageUsageDepthStencilAttachment = 0x20,
	ImageUsageDepthStencilReadOnly = 0x4000
	//ImageUsageInputAttachment = 128,
	//ImageUsageCPUVisible = 1048576
};
using ImageUsageFlags = uint32_t;

enum class ImageViewType {
	OneD = 0,
	TwoD = 1,
	ThreeD = 2,
	Cube = 3
};

enum class CompareOp : uint32_t {
	Never = 0,
	Less = 1,
	Equal = 2,
	LessOrEqual = 3,
	Greater = 4,
	NotEqual = 5,
	GreaterOrEqual = 6,
	Always = 7
};

enum class LogicOp : uint32_t {
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

enum ColorComponentFlagBits {
	ColorComponentR = 1,
	ColorComponentG = 2,
	ColorComponentB = 4,
	ColorComponentA = 8
};
using ColorComponentFlags = uint32_t;

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
	ImageUsageFlags previous_usage;
	ImageUsageFlags current_usage;
	ImageUsageFlags next_usage;
	Format format = Format::Undefined;
	InitialAction initial_action;
	FinalAction final_action;
	InitialAction stencil_initial_action;
	FinalAction stencil_final_action;
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

enum PipelineDynamicStateBits {
	DYNAMIC_STATE_VIEWPORT = 0,
	DYNAMIC_STATE_SCISSOR = 1,
	DYNAMIC_STATE_LINE_WIDTH = 2,
	DYNAMIC_STATE_DEPTH_BIAS = 3,
	DYNAMIC_STATE_DEPTH_BOUNDS = 5,
	DYNAMIC_STATE_STENCIL_COMPARE_MASK = 6,
	DYNAMIC_STATE_STENCIL_WRITE_MASK = 7,
	DYNAMIC_STATE_STENCIL_REFERENCE = 8,
};
using PipelineDynamicStateFlags = uint32_t;

struct DynamicStates {
	PipelineDynamicStateFlags* dynamic_states = nullptr;
	uint32_t dynamic_state_count = 0;
};

enum class StencilOp : uint32_t {
	Keep = 0,
	Zero = 1,
	Replace = 2,
	IncrementAndClamp = 3,
	DecrementAndClamp = 4,
	Invert = 5,
	IncrementAndWrap = 6,
	DecrementAndWrap = 7
};

enum class StencilFaces : uint32_t {
	Front = 1,
	Back = 2,
	FrontAndBack = 3
};

struct StencilOpState {
	StencilOp fail_op = StencilOp::Keep;
	StencilOp pass_op = StencilOp::Replace;
	StencilOp depth_fail_op = StencilOp::Keep;
	CompareOp compare_op = CompareOp::Always;
	uint32_t compare_mask = 0xFF;
	uint32_t write_mask = 0xFF;
	uint32_t reference = 111; // Garbage value
};

struct DepthStencilState {
	bool depth_test_enable = false;
	bool depth_write_enable = false;
	CompareOp depth_compare_op = CompareOp::LessOrEqual;
	bool depth_bounds_test_enable = false;
	bool stencil_test_enable = false;
	StencilOpState front;
	StencilOpState back;
	float min_depth_bounds = 0.0f;
	float max_depth_bounds = 1.0f;
};

enum class BlendFactor : uint32_t {
	Zero = 0,
	One = 1,
	SrcColor = 2,
	OneMinusSrcColor = 3,
	DstColor = 4,
	OneMinusDstColor = 5,
	SrcAlpha = 6,
	OneMinusSrcAlpha = 7,
	DstAlpha = 8,
	OneMinusDstAlpha = 9,
	ConstantColor = 10,
	OneMinusConstantColor = 11,
	ConstantAlpha = 12,
	OneMinusConstantAlpha = 13,
	SrcAlphaSaturate = 14,
	Src1Color = 15,
	OneMinusSrc1Color = 16,
	Src1Alpha = 17,
	OneMinusSrc1Alpha = 18
};

enum class BlendOp : uint32_t {
	Add = 0,
	Substract = 1,
	ReverseSubstract = 2,
	Min = 3,
	Max = 4
};

struct ColorBlendAttachmentState {
	bool blend_enable;
	BlendFactor src_color_blend_factor;
	BlendFactor dst_color_blend_factor;
	BlendOp color_blend_op;
	BlendFactor src_alpha_blend_factor;
	BlendFactor dst_alpha_blend_factor;
	BlendOp alpha_blend_op;
	ColorComponentFlags color_write_mask;
};

struct ColorBlendState {
	bool logic_op_enable;
	LogicOp logic_op;
	uint32_t attachment_count;
	ColorBlendAttachmentState* attachments;
	float blend_constants[4];
};

struct PipelineInfo {
	ShaderId shader_id;
	PipelineAssembly assembly;
	Rasterization raster;
	DepthStencilState depth_stencil;
	ColorBlendState color_blend;
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

struct ImageDataInfo {
	Format format;
	const void* data;
};

enum class IndexType : uint32_t {
	Uint16 = 0,
	Uint32 = 1
};

enum class BufferType : uint32_t {
	Vertex,
	Index,
	Uniform
};

struct BufferInfo {
	BufferType type;
	size_t offset;
	size_t size;
	IndexType index_type;
};

enum class UniformType : uint32_t {
	Sampler = 0,
	CombinedImageSampler = 1,
	SampledImage = 2,
	UniformBuffer = 6
};

struct UniformInfo {
	UniformType type;
	ImageUsageFlags image_usage; // In case uniform is texture
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

typedef union ClearValue {
	glm::vec4 color;
	struct {
		float depth;
		uint32_t stencil;
	} depth_stencil;
} ClearValue;

enum ShaderStageBits {
	ShaderStageVertex = 1,
	ShaderStageFragment = 16
};
using ShaderStageFlags = uint32_t;

struct ShaderStage {
	ShaderStageFlags stage;
	const void* spv;
	size_t spv_size;
};

struct ScreenResolution {
	uint32_t width;
	uint32_t height;
};

class VulkanGraphicsController {
public:
	void create(VulkanContext* context);
	void destroy();

	void end_frame();
	
	void draw_begin(FramebufferId framebuffer_id, const ClearValue* clear_values, uint32_t count);
	void draw_end();

	void draw_begin_for_screen(const glm::vec4& clear_color);
	void draw_end_for_screen();

	void draw_set_viewport(float x, float y, float width, float height, float min_depth, float max_depth);
	void draw_set_scissor(int x_offset, int y_offset, uint32_t width, uint32_t height);
	void draw_set_line_width(float width);
	void draw_set_stencil_reference(StencilFaces faces, uint32_t reference);

	void draw_push_constants(ShaderId shader, ShaderStageFlags stage, uint32_t offset, uint32_t size, const void* data);

	void draw_bind_pipeline(PipelineId pipeline_id);
	void draw_bind_vertex_buffer(BufferId buffer_id);
	void draw_bind_index_buffer(BufferId buffer_id, IndexType index_type);
	void draw_bind_uniform_sets(PipelineId pipeline_id, uint32_t first_set, const UniformSetId* set_ids, uint32_t count);

	void draw_draw_indexed(uint32_t index_count, uint32_t first_index);
	void draw_draw(uint32_t vertex_count, uint32_t first_vertex);

	RenderPassId render_pass_create(const RenderPassAttachment* attachments, RenderId count);

	FramebufferId framebuffer_create(RenderPassId render_pass_id, const ImageId* ids, uint32_t count);

	ShaderId shader_create(const ShaderStage* stages, RenderId stage_count);
	
	PipelineId pipeline_create(const PipelineInfo& pipeline_info);
	
	BufferId vertex_buffer_create(const void* data, size_t size);
	BufferId index_buffer_create(const void* data, size_t size, IndexType index_type);
	BufferId uniform_buffer_create(const void* data, size_t size);

	void buffer_update(BufferId buffer_id, const void* data);

	ImageId image_create(const ImageInfo& info);
	void image_update(ImageId image_id, const ImageDataInfo& image_data_info);

	SamplerId sampler_create(const SamplerInfo& info);

	UniformSetId uniform_set_create(ShaderId shader_id, uint32_t set_idx, const UniformInfo* uniforms, size_t uniform_count);

	ScreenResolution screen_resolution() const;

	void timestamp_query_begin();
	void timestamp_query_end();
	void timestamp_query_write_timestamp();
	bool timestamp_query_get_results(uint64_t *data, uint32_t count);

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
		std::vector<VkImageView> image_views;
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
		std::vector<VkPushConstantRange> push_constants;
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

	struct StagingBuffer {
		VkBuffer buffer;
		VkDeviceMemory memory;
	};

	VkBuffer buffer_create(VkBufferUsageFlags usage, VkDeviceSize size);
	StagingBuffer staging_buffer_create(const void* data, size_t size);
	VkDeviceMemory buffer_allocate(VkBuffer buffer, VkMemoryPropertyFlags mem_props);
	void buffer_copy(VkBuffer buffer, const void* data, VkDeviceSize size);
	void buffer_memory_barrier(VkBuffer& buffer, VkBufferUsageFlags usage, VkDeviceSize offset, VkDeviceSize size);

	// Images
	struct Image {
		ImageInfo info;
		VkImage image;
		VkDeviceMemory memory;
		VkImageLayout current_layout;
		VkImageAspectFlags full_aspect;
		VkImageTiling tiling;
	};

	VkImage vulkan_image_create(ImageViewType view_type, VkFormat format, VkExtent3D extent, uint32_t layer_count, VkImageTiling tiling, VkImageUsageFlags usage);
	VkDeviceMemory vulkan_image_allocate(VkImage image, VkMemoryPropertyFlags mem_props);
	VkImageView image_view_create(const Image& image, ImageUsageFlags image_usage);
	void vulkan_copy_buffer_to_image(VkImage image, StagingBuffer staging_buffer, VkExtent3D extent, VkImageAspectFlags aspect, VkImageLayout layout, uint32_t layer_count);
	void image_should_have_layout(Image& image, VkImageLayout layout);
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
		std::vector<VkImageView> image_views;
		DescriptorPoolKey pool_key;
		uint32_t pool_idx;
		ShaderId shader;
		uint32_t set_idx;
		VkDescriptorSet descriptor_set;
	};

	// Timestamp Query
	struct TimestampQueryPool {
		VkQueryPool pool;
		std::array<uint64_t, 128> query_data;
		uint32_t timestamps_written;
	};

private:
	struct Frame {
		VkCommandPool command_pool;
		VkCommandBuffer setup_buffer;
		VkCommandBuffer draw_buffer;
		std::vector<StagingBuffer> staging_buffers;
		TimestampQueryPool timestamp_query_pool;
	};

	VulkanContext* m_context;
	std::vector<Frame> m_frames;
	size_t m_frame_index;
	size_t m_frame_count;

	std::vector<Shader> m_shaders;
	std::vector<Pipeline> m_pipelines;
	std::vector<Buffer> m_buffers;
	std::vector<Image> m_images;
	std::vector<Sampler> m_samplers;
	std::map<DescriptorPoolKey, std::vector<DescriptorPool>> m_descriptor_pools;
	std::vector<UniformSet> m_uniform_sets;
};