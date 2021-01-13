#pragma once

#include "VulkanContext.h"

#include <string>

using RenderId = uint32_t;
using BufferId = uint32_t;
using ShaderId = uint32_t;
using UniformSetId = uint32_t;

enum class UniformTypeNew : uint32_t {
	Sampler = 0,
	CombinedImageSampler = 1,
	SampledImage = 2,
	UniformBuffer = 6
};

struct UniformInfo {
	UniformTypeNew type;
	uint32_t binding;
	RenderId id;
};

enum class DataFormat : uint32_t {
	Float,
	Float2,
	Float3,
	Float4,
	Uint,
	Uint2,
	Uint3,
	Uint4
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
	struct InputVar {
		uint32_t location;
		VkFormat format;
	};

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

		VkPipelineShaderStageCreateInfo vertex_create_info;
		VkPipelineShaderStageCreateInfo fragment_create_info;
		VkPipelineVertexInputStateCreateInfo vertex_input_create_info;
		VkPipelineLayout pipeline_layout;
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
	std::vector<Buffer> m_buffers;

	// TODO: Delete these lines
	VkRenderPass m_clear_render_pass;
	std::vector<VkFramebuffer> m_clear_framebuffers;
	// Delete down here
};