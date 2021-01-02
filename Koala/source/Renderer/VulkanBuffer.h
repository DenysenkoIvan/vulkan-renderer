#pragma once

#include "VulkanContext.h"

#include <vulkan/vulkan.h>

class VulkanBuffer {
public:
	VulkanBuffer(const void* data, uint64_t size, VkBufferUsageFlags usage);
	~VulkanBuffer();

	VkBuffer buffer() const { return m_buffer; }
	VkDeviceMemory memory() const { return m_buffer_memory; }

	void clear();
	void release_staging_buffer();

	static void set_context(std::shared_ptr<VulkanContext> context);

private:
	static std::shared_ptr<VulkanContext> s_context;

	VkBuffer m_buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_buffer_memory = VK_NULL_HANDLE;
	VkDeviceSize m_buffer_size = 0;

	VkBuffer m_staging_buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_staging_memory = VK_NULL_HANDLE;
};