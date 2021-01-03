#pragma once

#include "VulkanContext.h"

#include "Buffer.h"
#include "Material.h"

class Renderer {
public:
	void create(std::shared_ptr<VulkanContext> context);
	void destroy();

	void on_resize(uint32_t width, uint32_t height);

	void clear_screen(VkClearValue clear_value);
	void submit_geometry(VertexBuffer& vertex_buffer, IndexBuffer& index_buffer);
	void display();

private:
	void find_depth_format();
	void create_depth_resources();
	void create_render_passes();
	void create_draw_render_pass();
	void create_clear_screen_render_pass();
	
	void create_framebuffers();
	void create_draw_framebuffers();
	void create_clear_screen_framebuffers();
	
	void create_pipeline();

	void destroy_depth_resources();
	void destroy_render_passes();
	void destroy_framebuffers();

private:
	std::shared_ptr<VulkanContext> m_context;

	// Depth Resources
	VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
	VulkanImage m_depth_image;

	// Clear Screen
	VkRenderPass m_clear_screen_render_pass;
	std::vector<VkFramebuffer> m_clear_screen_framebuffers;

	// Render
	VkRenderPass m_draw_render_pass;
	std::vector<VkFramebuffer> m_draw_framebuffers;

	// TODO: Delete this line
	VkPipeline m_pipeline;
};