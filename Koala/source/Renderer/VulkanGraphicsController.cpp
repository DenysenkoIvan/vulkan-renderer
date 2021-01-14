#include "VulkanGraphicsController.h"

#include <spirv_reflect.h>

#include <algorithm>
#include <stdexcept>

void VulkanGraphicsController::create(VulkanContext* context) {
	m_context = context;

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
	
	VkAttachmentReference color_attachment_ref{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpass{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_ref
	};
	
	VkSubpassDependency dependency{
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT
	};
	
	VkRenderPassCreateInfo render_pass_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_attachment,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	};

	vkCreateRenderPass(m_context->device(), &render_pass_info, nullptr, &m_default_render_pass);

	create_framebuffer();
}

void VulkanGraphicsController::destroy() {

}

void VulkanGraphicsController::resize(uint32_t width, uint32_t height) {
	destroy_framebuffer();

	m_context->resize(width, height);

	create_framebuffer();
}

void VulkanGraphicsController::begin_frame() {
	VkClearValue clear_value;
	clear_value.color = { 1.0f, 0.0f, 1.0f, 1.0f };
	clear_value.depthStencil = { 1.0f, 0 };
	
	VkRenderPassBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = m_default_render_pass,
		.framebuffer = m_default_framebuffers[m_context->image_index()],
		.renderArea = {.extent = m_context->swapchain_extent() },
		.clearValueCount = 1,
		.pClearValues = &clear_value
	};
	
	VkCommandBuffer draw_buffer = m_context->draw_command_buffer();

	vkCmdBeginRenderPass(draw_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdEndRenderPass(draw_buffer);
}

void VulkanGraphicsController::end_frame() {
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
				
				stride += input_var->array.stride;

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
		.depthTestEnable = VK_FALSE,
		.depthWriteEnable = VK_FALSE,
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
	buffer.memory = memory_buffer_allocate(buffer.buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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

	buffer.buffer = buffer_create(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size);
	buffer.memory = memory_buffer_allocate(buffer.buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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

VkDeviceMemory VulkanGraphicsController::memory_buffer_allocate(VkBuffer buffer, VkMemoryPropertyFlags mem_props) {
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
	VkDeviceMemory staging_memory = memory_buffer_allocate(staging_buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

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

void VulkanGraphicsController::create_framebuffer() {
	uint32_t framebuffer_count = m_context->swapchain_image_count();
	m_default_framebuffers.resize(framebuffer_count);

	VkFramebufferCreateInfo framebuffer_info{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = m_default_render_pass,
		.attachmentCount = 1,
		.width = m_context->swapchain_extent().width,
		.height = m_context->swapchain_extent().height,
		.layers = 1
	};

	for (uint32_t i = 0; i < framebuffer_count; i++) {
		VkImageView view = m_context->swapchain_image_views()[i];

		framebuffer_info.pAttachments = &view;

		vkCreateFramebuffer(m_context->device(), &framebuffer_info, nullptr, &m_default_framebuffers[i]);
	}
}

void VulkanGraphicsController::destroy_framebuffer() {
	for (VkFramebuffer framebuffer : m_default_framebuffers)
		vkDestroyFramebuffer(m_context->device(), framebuffer, nullptr);

	m_default_framebuffers.clear();
}