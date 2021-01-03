#pragma once

#include "VulkanPhysicalDevice.h"
#include "VulkanDevice.h"

#include <functional>
#include <memory>
#include <vector>

struct BufferAllocationInfo {
	VkBuffer buffer;
	VkDeviceMemory buffer_memory;
};

struct ImageAllocationInfo {
	VkImage image;
	VkDeviceMemory image_memory;
	VkImageView image_view;
};

class VulkanAllocator {
public:
	VulkanAllocator() = default;

	void create(std::shared_ptr<VulkanPhysicalDevice> physical_device, std::shared_ptr<VulkanDevice> device);

	void set_submit_memory_commands_callback(const std::function<void(const std::function<void(VkCommandBuffer)>&)>& callback);

	void free_staging_buffers();

	BufferAllocationInfo allocate_buffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage);
	ImageAllocationInfo allocate_image(const void* data, uint32_t size, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, VkImageAspectFlags aspect);
	ImageAllocationInfo allocate_empty_image(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, VkImageAspectFlags aspect);

private:
	uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

	VkBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage) const;
	VkDeviceMemory allocate_buffer_memory(VkBuffer buffer, VkDeviceSize size, VkMemoryPropertyFlags properties) const;

	VkImage create_image(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage) const;
	VkDeviceMemory allocate_image_memory(VkImage, VkMemoryPropertyFlags mem_properties) const;
	VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect) const;

	void copy_buffer_to_image(VkBuffer buffer, VkImage image, VkExtent2D extent, VkImageLayout layout) const;
	void transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) const;

private:
	std::function<void(const std::function<void(VkCommandBuffer)>&)> m_submit_memory_commands_fun;

	struct StagingBuffer {
		VkBuffer buffer;
		VkDeviceMemory memory;
	};

	std::shared_ptr<VulkanPhysicalDevice> m_physical_device;
	std::shared_ptr<VulkanDevice> m_device;
	std::vector<StagingBuffer> m_staging_buffers;
};