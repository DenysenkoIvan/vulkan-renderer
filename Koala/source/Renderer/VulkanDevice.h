#pragma once

#include "VulkanPhysicalDevice.h"

#include <vulkan/vulkan.h>

class VulkanDevice {
public:
	void create(const VulkanPhysicalDevice& physical_device, const std::vector<const char*>& extensions);
	void destroy();

	VkDevice device() const { return m_device; }
	VkQueue graphics_queue() const { return m_graphics_queue; }
	VkQueue present_queue() const { return m_present_queue; }

private:
	void create_device(const VulkanPhysicalDevice& physical_device, const std::vector<const char*>& extensions);
	void retrieve_queues(uint32_t graphics_queue, uint32_t present_queue);

private:
	VkDevice m_device = VK_NULL_HANDLE;

	VkQueue m_graphics_queue = VK_NULL_HANDLE;
	VkQueue m_present_queue = VK_NULL_HANDLE;
};