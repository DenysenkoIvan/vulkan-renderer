#include "Renderer.h"

void Renderer::create(std::shared_ptr<VulkanContext> context) {
	m_context = context;

	create_render_pass();
	create_framebuffers();

	m_context->begin_frame();
}

void Renderer::destroy() {
	destroy_framebuffers();
	vkDestroyRenderPass(m_context->device().device(), m_render_pass, nullptr);
}

void Renderer::on_resize(uint32_t width, uint32_t height) {
	//m_context->end_frame();

	m_context->on_resize(width, height);
	destroy_framebuffers();
	create_framebuffers();

	m_context->begin_frame();
}

void Renderer::clear_screen(VkClearValue clear_value) {
	VkRenderPassBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = m_render_pass,
		.framebuffer = m_framebuffers[m_context->image_index()],
		.renderArea = { { 0, 0 }, m_context->swapchain().image_extent() },
		.clearValueCount = 1,
		.pClearValues = &clear_value
	};

	m_context->submit([&](VkCommandBuffer cb) {
		vkCmdBeginRenderPass(cb, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdEndRenderPass(cb);
	});
}

void Renderer::display() {
	m_context->end_frame();

	m_context->begin_frame();
}

void Renderer::create_render_pass() {
	VkAttachmentDescription color_attachment{
			.format = m_context->swapchain().format(),
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

	if (vkCreateRenderPass(m_context->device().device(), &renderpass_info, nullptr, &m_render_pass) != VK_SUCCESS)
		throw std::runtime_error("Failed to create render pass");
}

void Renderer::create_framebuffers() {
	m_framebuffers.resize(m_context->swapchain().image_count());

	for (size_t i = 0; i < m_framebuffers.size(); i++) {
		VkFramebufferCreateInfo framebuffer_info{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = m_render_pass,
			.attachmentCount = 1,
			.pAttachments = &m_context->swapchain().image_views()[i],
			.width = m_context->swapchain().image_extent().width,
			.height = m_context->swapchain().image_extent().height,
			.layers = 1
		};

		if (vkCreateFramebuffer(m_context->device().device(), &framebuffer_info, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create framebuffer");
	}
}

void Renderer::destroy_framebuffers() {
	for (VkFramebuffer framebuffer : m_framebuffers)
		vkDestroyFramebuffer(m_context->device().device(), framebuffer, nullptr);
	m_framebuffers.resize(0);
}