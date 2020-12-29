#pragma once

#include "vulkan/vulkan.h"

#include <cstdint>
#include <limits>
#include <vector>

struct QueueFamily {
	uint32_t index = ~0u;
	uint32_t count = ~0u;
};

class VulkanPhysicalDevice {
public:
	VulkanPhysicalDevice() = default;

	VkBool32 pick(VkInstance instance, const std::vector<const char*>& extensions, VkSurfaceKHR surface);

	VkPhysicalDevice physical_device() const { return m_physical_device; }

	QueueFamily graphics_family() const { return m_graphics_family; }
	QueueFamily present_family() const { return m_present_family; }

	const VkPhysicalDeviceProperties& properties() const { return m_properties; }
	const VkPhysicalDeviceMemoryProperties& memory_properties() const { return m_memory_properties; }
	const VkPhysicalDeviceFeatures& features() const { return m_features; }
	const VkPhysicalDeviceLimits& limits() const { return m_properties.limits; }
	
private:
	uint32_t calculate_priority(VkPhysicalDevice physical_device, const std::vector<const char*>& required_extensions, VkSurfaceKHR surface);
	bool has_extensions_support(VkPhysicalDevice physical_device, const std::vector<const char*>& extensions);
	bool has_complete_queue_families(VkPhysicalDevice physical_device, VkSurfaceKHR surface);
	bool has_surface_support(VkPhysicalDevice physical_device, uint32_t queue, VkSurfaceKHR surface);
	void save_physical_device(VkPhysicalDevice physical_device, VkSurfaceKHR surface);
	std::pair<QueueFamily, QueueFamily> retrieve_queue_families(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

private:
	VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;

	QueueFamily m_graphics_family;
	QueueFamily m_present_family;

	VkPhysicalDeviceProperties m_properties;
	VkPhysicalDeviceMemoryProperties m_memory_properties;
	VkPhysicalDeviceFeatures m_features;
};