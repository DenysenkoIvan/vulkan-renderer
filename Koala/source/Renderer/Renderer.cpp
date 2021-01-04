#include "Renderer.h"
#include "Shader.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"

#include <array>

void Renderer::create(std::shared_ptr<VulkanContext> context) {
	m_context = context;

	Shader::set_context(context);
	VulkanBuffer::set_context(context);
	VulkanImage::set_context(context);

	find_depth_format();
	create_depth_resources();
	create_render_passes();
	create_framebuffers();
	create_pipeline();
	
	m_context->begin_frame();
}

void Renderer::destroy() {
	vkDestroyPipeline(m_context->device().device(), m_pipeline, nullptr);
	destroy_depth_resources();
	destroy_framebuffers();
	destroy_render_passes();
}

void Renderer::on_resize(uint32_t width, uint32_t height) {
	//m_context->end_frame();

	m_context->on_resize(width, height);
	
	destroy_framebuffers();
	destroy_depth_resources();

	create_depth_resources();
	create_framebuffers();

	m_context->begin_frame();
}

void Renderer::submit_geometry(VertexBuffer& vertex_buffer, IndexBuffer& index_buffer) {
	VkRenderPassBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = m_draw_render_pass,
		.framebuffer = m_draw_framebuffers[m_context->swapchain().image_index()],
		.renderArea = { .offset = { 0, 0 }, .extent = m_context->swapchain().extent() }
	};

	VkBuffer vertex_buffers[] = { vertex_buffer.buffer().buffer() };
	VkDeviceSize offsets[] = { 0 };

	m_context->submit_render_commands([&](VkCommandBuffer cmd_buf) {
		vkCmdBeginRenderPass(cmd_buf, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
		vkCmdBindVertexBuffers(cmd_buf, 0, 1, vertex_buffers, offsets);
		vkCmdBindIndexBuffer(cmd_buf, index_buffer.buffer().buffer(), 0, (VkIndexType)index_buffer.type());
		vkCmdDrawIndexed(cmd_buf, index_buffer.index_count(), 1, 0, 0, 0);
		vkCmdEndRenderPass(cmd_buf);
	});
}

void Renderer::clear_screen(VkClearValue clear_value) {
	VkRenderPassBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = m_clear_screen_render_pass,
		.framebuffer = m_clear_screen_framebuffers[m_context->swapchain().image_index()],
		.renderArea = { { 0, 0 }, m_context->swapchain().extent() },
		.clearValueCount = 1,
		.pClearValues = &clear_value
	};

	m_context->submit_render_commands([&](VkCommandBuffer cb) {
		vkCmdBeginRenderPass(cb, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdEndRenderPass(cb);
	});
}

void Renderer::display() {
	m_context->end_frame();

	m_context->begin_frame();
}

void Renderer::find_depth_format() {
	VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
	VkFormatProperties props;

	props = m_context->physical_device().format_properties(VK_FORMAT_D24_UNORM_S8_UINT);
	if ((props.optimalTilingFeatures & features) == features) {
		m_depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
		return;
	}

	props = m_context->physical_device().format_properties(VK_FORMAT_D32_SFLOAT);
	if ((props.optimalTilingFeatures & features) == features) {
		m_depth_format = VK_FORMAT_D32_SFLOAT;
		return;
	}

	props = m_context->physical_device().format_properties(VK_FORMAT_D32_SFLOAT_S8_UINT);
	if ((props.optimalTilingFeatures & features) == features) {
		m_depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
		return;
	}
}

void Renderer::create_depth_resources() {
	m_depth_image.create(m_context->swapchain().extent(), m_depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Renderer::create_render_passes() {
	create_draw_render_pass();
	create_clear_screen_render_pass();
}

void Renderer::create_draw_render_pass() {
	VkAttachmentDescription color_attachment{
			.format = m_context->swapchain().format(),
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
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
		.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	std::array<VkAttachmentDescription, 1> attachments = { color_attachment };

	VkRenderPassCreateInfo render_pass_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = (uint32_t)attachments.size(),
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	};

	if (vkCreateRenderPass(m_context->device().device(), &render_pass_info, nullptr, &m_draw_render_pass) != VK_SUCCESS)
		throw std::runtime_error("Failed to crate draw render pass");
}

void Renderer::create_clear_screen_render_pass() {
	VkAttachmentDescription color_attachment{
			.format = m_context->swapchain().format(),
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
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
		.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	VkRenderPassCreateInfo renderpass_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_attachment,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	};

	if (vkCreateRenderPass(m_context->device().device(), &renderpass_info, nullptr, &m_clear_screen_render_pass) != VK_SUCCESS)
		throw std::runtime_error("Failed to create render pass");
}

void Renderer::create_framebuffers() {
	create_draw_framebuffers();
	create_clear_screen_framebuffers();
}

void Renderer::create_draw_framebuffers() {
	m_draw_framebuffers.resize(m_context->swapchain().image_count());

	VkFramebufferCreateInfo framebuffer_info{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = m_draw_render_pass,
		.width = m_context->swapchain().extent().width,
		.height = m_context->swapchain().extent().height,
		.layers = 1
	};

	for (size_t i = 0; i < m_draw_framebuffers.size(); i++) {
		std::array<VkImageView, 1> attachments = { m_context->swapchain().image_views()[i] };

		framebuffer_info.attachmentCount = (uint32_t)attachments.size();
		framebuffer_info.pAttachments = attachments.data();

		if (vkCreateFramebuffer(m_context->device().device(), &framebuffer_info, nullptr, &m_draw_framebuffers[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create draw framebuffer");
	}
}

void Renderer::create_clear_screen_framebuffers() {
	m_clear_screen_framebuffers.resize(m_context->swapchain().image_count());

	VkFramebufferCreateInfo framebuffer_info{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = m_clear_screen_render_pass,
		.attachmentCount = 1,
		.width = m_context->swapchain().extent().width,
		.height = m_context->swapchain().extent().height,
		.layers = 1
	};

	for (size_t i = 0; i < m_clear_screen_framebuffers.size(); i++) {
		framebuffer_info.pAttachments = &m_context->swapchain().image_views()[i];

		if (vkCreateFramebuffer(m_context->device().device(), &framebuffer_info, nullptr, &m_clear_screen_framebuffers[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create framebuffer");
	}
}

void Renderer::create_pipeline() {
	std::shared_ptr<Shader> shader = Shader::create("../assets/shaders/vertex.spv", "../assets/shaders/fragment.spv");
	
	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = shader->pipeline_shader_stage_infos();

	VkVertexInputBindingDescription vertex_binding_description{
		.binding = 0,
		.stride = 24,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions;
	attribute_descriptions[0].location = 0;
	attribute_descriptions[0].binding = 0;
	attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribute_descriptions[0].offset = 0;
	attribute_descriptions[1].location = 1;
	attribute_descriptions[1].binding = 0;
	attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribute_descriptions[1].offset = 12;

	VkPipelineVertexInputStateCreateInfo vertex_input_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertex_binding_description,
		.vertexAttributeDescriptionCount = (uint32_t)attribute_descriptions.size(),
		.pVertexAttributeDescriptions = attribute_descriptions.data()
	};

	VkPipelineInputAssemblyStateCreateInfo assembly_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE
	};

	VkViewport viewport{
		.x = 0,
		.y = 0,
		.width = (float)m_context->swapchain().extent().width,
		.height = (float)m_context->swapchain().extent().height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	
	VkRect2D scissor{
		.offset = { 0, 0 },
		.extent = m_context->swapchain().extent()
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
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f
	};
	
	VkPipelineMultisampleStateCreateInfo multisample_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE
	};

	VkPipelineColorBlendAttachmentState color_blend{
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	VkPipelineColorBlendStateCreateInfo blend_state{};
	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &color_blend;
	
	std::array<VkDynamicState, 2> dynamic_states{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamic_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = (uint32_t)dynamic_states.size(),
		.pDynamicStates = dynamic_states.data()
	};

	VkGraphicsPipelineCreateInfo pipeline_info{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = (uint32_t)shader_stages.size(),
		.pStages = shader_stages.data(),
		.pVertexInputState = &vertex_input_state,
		.pInputAssemblyState = &assembly_state,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterization_state,
		.pMultisampleState = &multisample_state,
		.pDepthStencilState = nullptr,
		.pColorBlendState = &blend_state,
		.pDynamicState = &dynamic_state,
		.layout = shader->pipeline_layout(),
		.renderPass = m_draw_render_pass,
		.subpass = 0
	};

	if (vkCreateGraphicsPipelines(m_context->device().device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline) != VK_SUCCESS)
		throw std::runtime_error("Failed to create graphics pipeline");
}

void Renderer::destroy_depth_resources() {
	m_depth_image.destroy();
}

void Renderer::destroy_render_passes() {
	vkDestroyRenderPass(m_context->device().device(), m_draw_render_pass, nullptr);
	vkDestroyRenderPass(m_context->device().device(), m_clear_screen_render_pass, nullptr);
}

void Renderer::destroy_framebuffers() {
	for (size_t i = 0; i < m_draw_framebuffers.size(); i++) {
		vkDestroyFramebuffer(m_context->device().device(), m_draw_framebuffers[i], nullptr);
		vkDestroyFramebuffer(m_context->device().device(), m_clear_screen_framebuffers[i], nullptr);
	}
	m_draw_framebuffers.resize(0);
	m_clear_screen_framebuffers.resize(0);
}