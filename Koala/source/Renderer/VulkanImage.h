#pragma once

#include "VulkanContext.h"

class VulkanImage {
public:
	~VulkanImage();

	void create(const void* data, uint32_t size, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, VkImageAspectFlags aspect);
	void create(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, VkImageAspectFlags aspect);

	void destroy();

	VkImage image() const { return m_image; }
	VkImageView view() const { return m_image_view; }
	VkDeviceMemory memory() const { return m_image_memory; }
	VkExtent2D extent() const { return m_extent; }
	VkImageLayout layout() const { return m_layout; }

	static void set_context(std::shared_ptr<VulkanContext> context);

private:
	static std::shared_ptr<VulkanContext> s_context;

	VkExtent2D m_extent;
	VkImageLayout m_layout;

	VkImage m_image;
	VkImageView m_image_view;
	VkDeviceMemory m_image_memory;
};