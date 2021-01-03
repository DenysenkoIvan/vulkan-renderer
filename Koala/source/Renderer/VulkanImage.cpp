#include "VulkanImage.h"

std::shared_ptr<VulkanContext> VulkanImage::s_context;

void VulkanImage::set_context(std::shared_ptr<VulkanContext> context) {
	s_context = context;
}

void VulkanImage::create(const void* data, uint32_t size, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, VkImageAspectFlags aspect) {
	if (m_image != VK_NULL_HANDLE)
		destroy();

	ImageAllocationInfo alloca_info = s_context->allocator().allocate_image(data, size, extent, format, usage, layout, aspect);

	m_extent = extent;

	m_image = alloca_info.image;
	m_image_memory = alloca_info.image_memory;
	m_image_view = alloca_info.image_view;
}

void VulkanImage::create(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, VkImageAspectFlags aspect) {
	if (m_image != VK_NULL_HANDLE)
		destroy();

	ImageAllocationInfo alloca_info = s_context->allocator().allocate_empty_image(extent, format, usage, layout, aspect);

	m_extent = extent;

	m_image = alloca_info.image;
	m_image_memory = alloca_info.image_memory;
	m_image_view = alloca_info.image_view;
}

VulkanImage::~VulkanImage() {
	destroy();
}

void VulkanImage::destroy() {
	if (m_image == VK_NULL_HANDLE) return;

	vkDestroyImageView(s_context->device().device(), m_image_view, nullptr);
	vkDestroyImage(s_context->device().device(), m_image, nullptr);
	vkFreeMemory(s_context->device().device(), m_image_memory, nullptr);

	m_image = VK_NULL_HANDLE;
	m_image_memory = VK_NULL_HANDLE;
	m_image_view = VK_NULL_HANDLE;
}