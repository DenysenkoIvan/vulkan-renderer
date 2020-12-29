#include "VulkanDevice.h"

#include <array>
#include <stdexcept>

void VulkanDevice::create(const VulkanPhysicalDevice& physical_device, const std::vector<const char*>& extensions) {
	uint32_t graphics_queue = physical_device.graphics_family().index;
	uint32_t present_queue = physical_device.present_family().index;
	
	create_device(physical_device, extensions);
	retrieve_queues(graphics_queue, present_queue);
}

void VulkanDevice::destroy() {
	vkDestroyDevice(m_device, nullptr);
}

void VulkanDevice::create_device(const VulkanPhysicalDevice& physical_device, const std::vector<const char*>& extensions) {
	float priority = 1;

	auto create_queue_create_info = [&priority](uint32_t index) -> VkDeviceQueueCreateInfo {
		return {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = index,
			.queueCount = 1,
			.pQueuePriorities = &priority
		};
	};

	uint32_t graphics_queue = physical_device.graphics_family().index;
	uint32_t present_queue = physical_device.present_family().index;

	std::array<VkDeviceQueueCreateInfo, 2> queue_infos{
		create_queue_create_info(graphics_queue),
		create_queue_create_info(present_queue)
	};

	uint32_t queue_count = (graphics_queue == present_queue ? 1 : 2);

	VkDeviceCreateInfo device_info{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = queue_count,
		.pQueueCreateInfos = queue_infos.data(),
		.enabledExtensionCount = (uint32_t)extensions.size(),
		.ppEnabledExtensionNames = extensions.data()
	};

	if (vkCreateDevice(physical_device.physical_device(), &device_info, nullptr, &m_device) != VK_SUCCESS)
		throw std::runtime_error("Failed to create Vulkan Device");
}

void VulkanDevice::retrieve_queues(uint32_t graphics_queue, uint32_t present_queue) {
	vkGetDeviceQueue(m_device, graphics_queue, 0, &m_graphics_queue);

	if (graphics_queue == present_queue)
		m_present_queue = m_graphics_queue;
	else
		vkGetDeviceQueue(m_device, present_queue, 0, &m_present_queue);
}