#pragma once

#include "VulkanPhysicalDevice.h"
#include "VulkanDevice.h"

#include <functional>
#include <memory>

struct BufferAllocationInfo {
	VkBuffer buffer;
	VkDeviceMemory buffer_memory;
	VkBuffer staging_buffer;
	VkDeviceMemory staging_memory;
};

class VulkanAllocator {
public:
	VulkanAllocator() = default;

	void create(std::shared_ptr<VulkanPhysicalDevice> physical_device, std::shared_ptr<VulkanDevice> device);

	void set_submit_memory_commands_callback(const std::function<void(const std::function<void(VkCommandBuffer)>&)>& callback);

	BufferAllocationInfo allocate_buffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage);

private:
	uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	VkBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage);
	VkDeviceMemory allocate_memory(VkBuffer buffer, VkDeviceSize size, VkMemoryPropertyFlags properties);

private:
	std::function<void(const std::function<void(VkCommandBuffer)>&)> m_submit_memory_commands_fun;

	std::shared_ptr<VulkanPhysicalDevice> m_physical_device;
	std::shared_ptr<VulkanDevice> m_device;
};