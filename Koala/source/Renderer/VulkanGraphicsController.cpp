#include "VulkanGraphicsController.h"

#include <spirv_reflect.h>

#include <algorithm>
#include <stdexcept>

static uint32_t vk_format_to_size(VkFormat format) {
	switch (format) {
	case VK_FORMAT_R32_SFLOAT:			return 1 * 4;
	case VK_FORMAT_R32G32_SFLOAT:		return 2 * 4;
	case VK_FORMAT_R32G32B32_SFLOAT:	return 3 * 4;
	case VK_FORMAT_R32G32B32A32_SFLOAT: return 4 * 4;
	}

	throw std::runtime_error("Unknown format");
}

void VulkanGraphicsController::create(VulkanContext* context) {
	m_context = context;

	find_depth_format();

	create_depth_resources();

	create_render_pass();

	create_framebuffer();
}

void VulkanGraphicsController::destroy() {
	destroy_framebuffer();
	vkDestroyRenderPass(m_context->device(), m_default_render_pass, nullptr);
	destroy_depth_resources();
}

void VulkanGraphicsController::resize(uint32_t width, uint32_t height) {
	destroy_framebuffer();
	destroy_depth_resources();

	m_context->resize(width, height);

	create_depth_resources();
	create_framebuffer();
}

void VulkanGraphicsController::begin_frame() {
	m_draw_calls.clear();
}

void VulkanGraphicsController::submit(PipelineId pipeline_id, BufferId vertex_id, BufferId index_id, const std::vector<UniformSetId>& uniform_sets) {
	// TODO: Add support for uniform sets
	m_draw_calls.emplace_back(pipeline_id, vertex_id, index_id, uniform_sets);
}

void VulkanGraphicsController::end_frame() {
	VkCommandBuffer draw_buffer = m_context->draw_command_buffer();

	std::array<VkClearValue, 2> clear_values;
	// Color attachment
	clear_values[0].color = { 1.0f, 0.0f, 1.0f, 1.0f };
	// Depth attachment
	clear_values[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = m_default_render_pass,
		.framebuffer = m_default_framebuffers[m_context->image_index()],
		.renderArea = { .extent = m_context->swapchain_extent() },
		.clearValueCount = (uint32_t)clear_values.size(),
		.pClearValues = clear_values.data()
	};

	vkCmdBeginRenderPass(draw_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	
	VkViewport viewport{
		.x = 0,
		.y = 0,
		.width = (float)m_context->swapchain_extent().width,
		.height = (float)m_context->swapchain_extent().height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	VkRect2D scissor{
		.offset = { 0, 0 },
		.extent = m_context->swapchain_extent()
	};

	vkCmdSetViewport(draw_buffer, 0, 1, &viewport);
	vkCmdSetScissor(draw_buffer, 0, 1, &scissor);

	for (DrawCall& draw_call : m_draw_calls) {
		vkCmdBindPipeline(draw_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[draw_call.pipeline].pipeline);
		VkBuffer vertex_buffers[1] = { m_buffers[draw_call.vertex_buffer].buffer };
		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(draw_buffer, 0, 1, vertex_buffers, offsets);
		vkCmdBindIndexBuffer(draw_buffer, m_buffers[draw_call.index_buffer].buffer, 0, m_buffers[draw_call.index_buffer].index.index_type);
		vkCmdDrawIndexed(draw_buffer, m_buffers[draw_call.index_buffer].index.index_count, 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(draw_buffer);
	
	m_context->swap_buffers();
}

ShaderId VulkanGraphicsController::shader_create(const std::vector<uint8_t>& vertex_spv, const std::vector<uint8_t>& fragment_spv) {
	m_shaders.push_back({});
	Shader& shader = m_shaders.back();
	shader.info = std::make_unique<ShaderInfo>();

	auto reflect_shader_stage = [this, &shader](const std::vector<uint8_t>& spv) {
		spv_reflect::ShaderModule shader_module(spv);

		VkShaderStageFlags shader_stage = (VkShaderStageFlags)shader_module.GetShaderStage();

		// Reflect Input Variables
		if (shader_stage == VK_SHADER_STAGE_VERTEX_BIT) {
			// Save vertex entry name
			shader.info->vertex_entry = shader_module.GetEntryPointName();

			uint32_t input_var_count = 0;
			shader_module.EnumerateInputVariables(&input_var_count, nullptr);
			std::vector<SpvReflectInterfaceVariable*> input_vars(input_var_count);
			shader_module.EnumerateInputVariables(&input_var_count, input_vars.data());

			shader.info->attribute_descriptions.reserve(input_var_count);

			uint32_t stride = 0;
			for (uint32_t i = 0; i < input_var_count; i++) {	
				SpvReflectInterfaceVariable* input_var = input_vars[i];

				VkVertexInputAttributeDescription attribute_description{
					.location = input_var->location,
					.binding = 0,
					.format = (VkFormat)input_var->format,
					.offset = stride
				};
				
				stride += vk_format_to_size((VkFormat)input_var->format);

				shader.info->attribute_descriptions.push_back(attribute_description);
			}

			shader.info->binding_description.binding = 0;
			shader.info->binding_description.stride = stride;
		} else {
			// Save fragment entry name
			shader.info->fragment_entry = shader_module.GetEntryPointName();
		}

		// Relfect Uniforms
		uint32_t binding_count = 0;
		shader_module.EnumerateDescriptorBindings(&binding_count, nullptr);
		std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
		shader_module.EnumerateDescriptorBindings(&binding_count, bindings.data());

		for (SpvReflectDescriptorBinding* descriptor_binding : bindings) {
			uint32_t set_idx = descriptor_binding->set;
			uint32_t binding_idx = descriptor_binding->binding;
			VkDescriptorType type = (VkDescriptorType)descriptor_binding->descriptor_type;
			uint32_t count = descriptor_binding->count;

			auto set_it = shader.info->find_set(set_idx);
			if (set_it == shader.info->sets.end()) {
				shader.info->sets.push_back({});
				set_it = shader.info->sets.end() - 1;
			}

			auto binding_it = set_it->find_binding(binding_idx);
			// Insert new binding, no problem
			if (binding_it == set_it->bindings.end()) {
				VkDescriptorSetLayoutBinding layout_binding{
					.binding = binding_idx,
					.descriptorType = type,
					.descriptorCount = count,
					.stageFlags = shader_stage
				};
				
				set_it->bindings.emplace_back(layout_binding);
			// Update binding stage and verify bindings are the same
			} else {
				binding_it->stageFlags |= shader_stage;

				if (type != binding_it->descriptorType)
					throw std::runtime_error("Uniform set binding redefinition with different descriptor type");
				if (count != binding_it->descriptorCount)
					throw std::runtime_error("Uniform set binding refefinition with different count");
			}
		}
	};

	reflect_shader_stage(vertex_spv);
	reflect_shader_stage(fragment_spv);

	// Create Vertex stage VkShaderModule
	VkShaderModuleCreateInfo vert_module_info{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = (uint32_t)vertex_spv.size(),
		.pCode = (uint32_t*)vertex_spv.data()
	};
	
	if (vkCreateShaderModule(m_context->device(), &vert_module_info, nullptr, &shader.info->vertex_module) != VK_SUCCESS)
		throw std::runtime_error("Failed to create Vertex stage VkShaderModule");

	// Create Fragment stage VkShaderModule
	VkShaderModuleCreateInfo frag_module_info{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = (uint32_t)fragment_spv.size(),
		.pCode = (uint32_t*)fragment_spv.data()
	};

	if (vkCreateShaderModule(m_context->device(), &frag_module_info, nullptr, &shader.info->fragment_module) != VK_SUCCESS)
		throw std::runtime_error("Failed to create Vertex stage VkShaderModule");

	// Create Vertex stage VkPipelineShaderStageCreateInfo
	shader.stage_create_infos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader.stage_create_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader.stage_create_infos[0].module = shader.info->vertex_module;
	shader.stage_create_infos[0].pName = shader.info->vertex_entry.c_str();

	// Create Fragment stage VkPipelineShaderStageCreateInfo
	shader.stage_create_infos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader.stage_create_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader.stage_create_infos[1].module = shader.info->fragment_module;
	shader.stage_create_infos[1].pName = shader.info->fragment_entry.c_str();

	// Create VkPipelineVertexInputStateCreateInfo
	shader.vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	shader.vertex_input_create_info.vertexBindingDescriptionCount = 1;
	shader.vertex_input_create_info.pVertexBindingDescriptions = &shader.info->binding_description;
	shader.vertex_input_create_info.vertexAttributeDescriptionCount = (uint32_t)shader.info->attribute_descriptions.size();
	shader.vertex_input_create_info.pVertexAttributeDescriptions = shader.info->attribute_descriptions.data();

	std::sort(shader.info->sets.begin(), shader.info->sets.end(), [](const auto& s0, const auto& s1) {
		return s0.set < s1.set;
	});

	// Create VkDescriptorSetLayout's
	size_t set_count = shader.info->sets.size();

	shader.info->set_layouts.reserve(set_count);
	for (size_t i = 0; i < set_count; i++) {
		Set& set = shader.info->sets[i];

		std::sort(set.bindings.begin(), set.bindings.end(), [](const auto& b0, const auto& b1) {
			return b0.binding < b1.binding;
		});

		VkDescriptorSetLayoutCreateInfo set_layout_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = (uint32_t)set.bindings.size(),
			.pBindings = set.bindings.data()
		};
	
		VkDescriptorSetLayout set_layout;
		if (vkCreateDescriptorSetLayout(m_context->device(), &set_layout_info, nullptr, &set_layout) != VK_SUCCESS)
			throw std::runtime_error("Failed to create descritptor set layout");

		shader.info->set_layouts.push_back(set_layout);
	}

	// Create pipeline layout
	VkPipelineLayoutCreateInfo pipeline_layout_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = (uint32_t)shader.info->set_layouts.size(),
		.pSetLayouts = shader.info->set_layouts.data()
	};

	if (vkCreatePipelineLayout(m_context->device(), &pipeline_layout_info, nullptr, &shader.pipeline_layout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create pipelien layout");

	return (ShaderId)m_shaders.size() - 1;
}

PipelineId VulkanGraphicsController::pipeline_create(const PipelineInfo* pipeline_info) {
	m_pipelines.push_back({});
	Pipeline& pipeline = m_pipelines.back();
	pipeline.info = *pipeline_info;

	const Shader& shader = m_shaders[pipeline.info.shader];

	VkPipelineInputAssemblyStateCreateInfo assembly_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = (VkPrimitiveTopology)pipeline_info->assembly.topology,
		.primitiveRestartEnable = pipeline_info->assembly.restart_enable
	};

	VkViewport viewport{
		.x = 0,
		.y = 0,
		.width = (float)m_context->swapchain_extent().width,
		.height = (float)m_context->swapchain_extent().height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	VkRect2D scissor{
		.offset = { 0, 0 },
		.extent = m_context->swapchain_extent()
	};

	VkPipelineViewportStateCreateInfo viewport_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};

	VkPipelineRasterizationStateCreateInfo rasterization_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = pipeline_info->raster.depth_clamp_enable,
		.rasterizerDiscardEnable = pipeline_info->raster.rasterizer_discard_enable,
		.polygonMode = (VkPolygonMode)pipeline_info->raster.polygon_mode,
		.cullMode = (VkCullModeFlags)pipeline_info->raster.cull_mode,
		.frontFace = (VkFrontFace)pipeline_info->raster.front_face,
		.depthBiasEnable = pipeline_info->raster.depth_bias_enable,
		.depthBiasConstantFactor = pipeline_info->raster.depth_bias_constant_factor,
		.depthBiasClamp = pipeline_info->raster.depth_bias_clamp,
		.depthBiasSlopeFactor =  pipeline_info->raster.detpth_bias_slope_factor,
		.lineWidth = pipeline_info->raster.line_width
	};

	VkPipelineMultisampleStateCreateInfo multisample_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE
	};

	// TODO: Depth Test
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE
	};
	
	VkPipelineColorBlendAttachmentState blend_attachment{
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	VkPipelineColorBlendStateCreateInfo color_blend_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &blend_attachment,
		.blendConstants = { 0, 0, 0, 0 }
	};

	std::array<VkDynamicState, 2> dynamic_states{
		VK_DYNAMIC_STATE_VIEWPORT , VK_DYNAMIC_STATE_SCISSOR 
	};

	VkPipelineDynamicStateCreateInfo dynamic_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = (uint32_t)dynamic_states.size(),
		.pDynamicStates = dynamic_states.data()
	};

	VkGraphicsPipelineCreateInfo pipeline_create_info{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = (uint32_t)shader.stage_create_infos.size(),
		.pStages = shader.stage_create_infos.data(),
		.pVertexInputState = &shader.vertex_input_create_info,
		.pInputAssemblyState = &assembly_state,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterization_state,
		.pMultisampleState = &multisample_state,
		.pDepthStencilState = &depth_stencil_state,
		.pColorBlendState = &color_blend_state,
		.pDynamicState = &dynamic_state,
		.layout = shader.pipeline_layout,
		.renderPass = m_default_render_pass,
		.subpass = 0
	};
	
	if (vkCreateGraphicsPipelines(m_context->device(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline.pipeline) != VK_SUCCESS)
		throw std::runtime_error("Failed to create graphics pipeline");

	return (PipelineId)m_pipelines.size() - 1;
}

BufferId VulkanGraphicsController::vertex_buffer_create(const void* data, size_t size) {
	Buffer buffer{
		.size = size,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
	};
	
	buffer.buffer = buffer_create(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size);
	buffer.memory = buffer_allocate(buffer.buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	buffer_copy(data, size, buffer.buffer);

	m_buffers.push_back(buffer);

	return (BufferId)m_buffers.size() - 1;
}

BufferId VulkanGraphicsController::index_buffer_create(const void* data, size_t size, IndexType index_type) {
	Buffer buffer{
		.size = size,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT
	};
	buffer.index.index_type = (VkIndexType)index_type;
	buffer.index.index_count = index_type == IndexType::Uint16 ? (uint32_t)size / 2 : (uint32_t)size / 4;

	buffer.buffer = buffer_create(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size);
	buffer.memory = buffer_allocate(buffer.buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	buffer_copy(data, size, buffer.buffer);

	m_buffers.push_back(buffer);

	return (BufferId)m_buffers.size() - 1;
}

BufferId VulkanGraphicsController::uniform_buffer_create(const void* data, size_t size) {
	Buffer buffer{
		.size = size,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
	};

	buffer.buffer = buffer_create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size);
	buffer.memory = buffer_allocate(buffer.buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	buffer_copy(data, size, buffer.buffer);

	m_buffers.push_back(buffer);

	return (BufferId)m_buffers.size() - 1;
}




VkBuffer VulkanGraphicsController::buffer_create(VkBufferUsageFlags usage, VkDeviceSize size) {
	VkBuffer buffer;

	VkBufferCreateInfo buffer_info{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	if (vkCreateBuffer(m_context->device(), &buffer_info, nullptr, &buffer) != VK_SUCCESS)
		throw std::runtime_error("Failed to create buffer");

	return buffer;
}

VkDeviceMemory VulkanGraphicsController::buffer_allocate(VkBuffer buffer, VkMemoryPropertyFlags mem_props) {
	VkMemoryRequirements mem_requirements;
	vkGetBufferMemoryRequirements(m_context->device(), buffer, &mem_requirements);

	VkMemoryAllocateInfo allocate_info{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_requirements.size,
		.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, mem_props)
	};

	VkDeviceMemory memory;
	if (vkAllocateMemory(m_context->device(), &allocate_info, nullptr, &memory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate buffer memory");

	vkBindBufferMemory(m_context->device(), buffer, memory, 0);

	return memory;
}

void VulkanGraphicsController::buffer_copy(const void* data, VkDeviceSize size, VkBuffer buffer) {
	VkBuffer staging_buffer = buffer_create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size);
	VkDeviceMemory staging_memory = buffer_allocate(staging_buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* staging_data = nullptr;
	vkMapMemory(m_context->device(), staging_memory, 0, size, 0, &staging_data);
	memcpy(staging_data, data, size);
	vkUnmapMemory(m_context->device(), staging_memory);

	VkCommandBuffer mem_buffer = m_context->memory_command_buffer();

	VkBufferCopy region{
		.size = size
	};

	vkCmdCopyBuffer(mem_buffer, staging_buffer, buffer, 1, &region);

	m_context->submit_staging_buffer(staging_buffer, staging_memory);
}

uint32_t VulkanGraphicsController::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
	const VkPhysicalDeviceMemoryProperties& mem_props = m_context->physical_device_mem_props();

	for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
		if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type");
}

VkImage VulkanGraphicsController::image_create(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage) {
	VkImageCreateInfo image_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = { extent.width, extent.height, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	
	VkImage image;
	if (vkCreateImage(m_context->device(), &image_info, nullptr, &image) != VK_SUCCESS)
		throw std::runtime_error("Failed to create image");
	
	return image;
}

VkDeviceMemory VulkanGraphicsController::image_allocate(VkImage image, VkMemoryPropertyFlags mem_props) {
	VkMemoryRequirements mem_reqs;
	vkGetImageMemoryRequirements(m_context->device(), image, &mem_reqs);
	
	VkMemoryAllocateInfo memory_info{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, mem_props)
	};

	VkDeviceMemory memory;
	if (vkAllocateMemory(m_context->device(), &memory_info, nullptr, &memory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate image memory");

	vkBindImageMemory(m_context->device(), image, memory, 0);

	return memory;
}

VkImageView VulkanGraphicsController::image_view_create(VkImage image, VkFormat format, VkImageAspectFlags aspect) {
	VkImageViewCreateInfo view_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange = { aspect, 0, 1, 0, 1 }
	};
	
	VkImageView image_view;
	if (vkCreateImageView(m_context->device(), &view_info, nullptr, &image_view) != VK_SUCCESS)
		throw std::runtime_error("Failed to create image view");

	return image_view;
}

void VulkanGraphicsController::find_depth_format() {
	VkPhysicalDevice physical_device = m_context->physical_device();
	
	VkFormat desirable_depth_format = VK_FORMAT_D32_SFLOAT;
	VkFormatFeatureFlags format_features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkFormatProperties format_props;
	vkGetPhysicalDeviceFormatProperties(physical_device, desirable_depth_format, &format_props);
	if (format_props.optimalTilingFeatures & format_features) {
		m_depth_format = desirable_depth_format;
		return;
	}

	desirable_depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
	vkGetPhysicalDeviceFormatProperties(physical_device, desirable_depth_format, &format_props);
	if (format_props.optimalTilingFeatures & format_features) {
		m_depth_format = desirable_depth_format;
		return;
	}

	desirable_depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
	vkGetPhysicalDeviceFormatProperties(physical_device, desirable_depth_format, &format_props);
	if (format_props.optimalTilingFeatures & format_features) {
		m_depth_format = desirable_depth_format;
		return;
	}

	throw std::runtime_error("Failed to find depth format");
}

void VulkanGraphicsController::create_depth_resources() {
	m_depth_images.resize(m_context->swapchain_image_count());

	VkExtent2D depth_extent = m_context->swapchain_extent();
	VkFormat depth_format = m_depth_format;
	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (depth_format == VK_FORMAT_D24_UNORM_S8_UINT || depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT)
		aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

	for (Image& image : m_depth_images) {
		image.extent = depth_extent;
		image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		image.image = image_create(depth_extent, depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
		image.memory = image_allocate(image.image, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);
		image.view = image_view_create(image.image, depth_format, aspect);
	}
}

void VulkanGraphicsController::destroy_depth_resources() {
	VkDevice device = m_context->device();

	for (Image& image : m_depth_images) {
		vkDestroyImageView(device, image.view, nullptr);
		vkDestroyImage(device, image.image, nullptr);
		vkFreeMemory(device, image.memory, nullptr);
	}
}

void VulkanGraphicsController::create_render_pass() {
	VkAttachmentDescription color_attachment{
		.format = m_context->swapchain_format(),
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	VkAttachmentDescription depth_attachment{
		.format = m_depth_format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	VkAttachmentReference color_attachment_ref{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkAttachmentReference depth_attachment_ref{
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpass{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_ref,
		.pDepthStencilAttachment = &depth_attachment_ref
	};

	VkSubpassDependency dependency{
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
	};

	std::array<VkAttachmentDescription, 2> attachments{
		color_attachment,
		depth_attachment
	};

	VkRenderPassCreateInfo render_pass_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = (uint32_t)attachments.size(),
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	};

	vkCreateRenderPass(m_context->device(), &render_pass_info, nullptr, &m_default_render_pass);
}

void VulkanGraphicsController::create_framebuffer() {
	uint32_t framebuffer_count = m_context->swapchain_image_count();
	m_default_framebuffers.resize(framebuffer_count);

	VkFramebufferCreateInfo framebuffer_info{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = m_default_render_pass,
		.attachmentCount = 2,
		.width = m_context->swapchain_extent().width,
		.height = m_context->swapchain_extent().height,
		.layers = 1
	};

	for (uint32_t i = 0; i < framebuffer_count; i++) {
		std::array<VkImageView, 2> views = {
			m_context->swapchain_image_views()[i],
			m_depth_images[i].view
		};

		framebuffer_info.pAttachments = views.data();

		vkCreateFramebuffer(m_context->device(), &framebuffer_info, nullptr, &m_default_framebuffers[i]);
	}
}

void VulkanGraphicsController::destroy_framebuffer() {
	for (VkFramebuffer framebuffer : m_default_framebuffers)
		vkDestroyFramebuffer(m_context->device(), framebuffer, nullptr);

	m_default_framebuffers.clear();
}