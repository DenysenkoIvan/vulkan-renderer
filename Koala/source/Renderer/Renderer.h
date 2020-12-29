#pragma once

#include "VulkanContext.h"

class Renderer {
public:
	void create(std::shared_ptr<VulkanContext> context);
	void destroy();

	void on_resize(uint32_t width, uint32_t height);

	void clear_screen(VkClearValue clear_value);
	void display();

private:
	void create_render_pass();
	void create_framebuffers();
	void destroy_framebuffers();

private:
	std::shared_ptr<VulkanContext> m_context;
	VkRenderPass m_render_pass;
	std::vector<VkFramebuffer> m_framebuffers;
};