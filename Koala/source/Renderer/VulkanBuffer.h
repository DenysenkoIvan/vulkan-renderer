#pragma once

#include "VulkanContext.h"

#include <vulkan/vulkan.h>

class VulkanBuffer {
public:
	~VulkanBuffer();

	void create(const void* data, uint64_t size, VkBufferUsageFlags usage);

	VkBuffer buffer() const { return m_buffer; }
	VkDeviceMemory memory() const { return m_buffer_memory; }
	VkDeviceSize size() const { return m_buffer_size; }

	void update(const void* data, uint64_t size);

	void clear();

	static void set_context(std::shared_ptr<VulkanContext> context);

private:
	static std::shared_ptr<VulkanContext> s_context;

	VkBuffer m_buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_buffer_memory = VK_NULL_HANDLE;
	
	VkDeviceSize m_buffer_size = 0;
	VkBufferUsageFlags m_usage = 0;
};