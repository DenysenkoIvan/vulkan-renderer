#include "VulkanPhysicalDevice.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

VkBool32 VulkanPhysicalDevice::pick(VkInstance instance, const std::vector<const char*>& extensions, VkSurfaceKHR surface) {
	uint32_t physical_devices_count = 0;
	vkEnumeratePhysicalDevices(instance, &physical_devices_count, nullptr);

	std::vector<VkPhysicalDevice> physical_devices(physical_devices_count);
	vkEnumeratePhysicalDevices(instance, &physical_devices_count, physical_devices.data());

	if (physical_devices.size() == 0) throw std::runtime_error("Failed to find GPU with Vulkan support");

	std::vector<uint32_t> priorities;

	for (auto physical_device : physical_devices) {
		uint32_t priority = calculate_priority(physical_device, extensions, surface);

		priorities.push_back(priority);
	}

	auto device_it = std::max_element(priorities.begin(), priorities.end());
	
	if (*device_it == 0)
		return VK_FALSE;
	
	size_t physical_device_index = std::distance(priorities.begin(), device_it);
	save_physical_device(physical_devices[physical_device_index], surface);

	return VK_SUCCESS;
}

VkFormatProperties VulkanPhysicalDevice::format_properties(VkFormat format) const {
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &props);

	return props;
}

uint32_t VulkanPhysicalDevice::calculate_priority(VkPhysicalDevice physical_device, const std::vector<const char*>& required_extensions, VkSurfaceKHR surface) {
	if (!has_extensions_support(physical_device, required_extensions)) return 0;
	if (!has_complete_queue_families(physical_device, surface)) return 0;
	
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physical_device, &properties);

	uint32_t priority = 0;

	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		priority += 1000;
	if (properties.apiVersion == VK_VERSION_1_2)
		priority += 200;
	else if (properties.apiVersion == VK_VERSION_1_1)
		priority += 100;

	return priority;
}

bool VulkanPhysicalDevice::has_extensions_support(VkPhysicalDevice physical_device, const std::vector<const char*>& required_extensions) {
	uint32_t extensions_count = 0;
	vkEnumerateDeviceExtensionProperties(physical_device, "", &extensions_count, nullptr);
	std::vector<VkExtensionProperties> extensions(extensions_count);
	vkEnumerateDeviceExtensionProperties(physical_device, "", &extensions_count, extensions.data());

	bool has_support = false;
	for (const char* required_extension : required_extensions) {
		has_support = false;

		for (const auto& ext_prop : extensions) {
			if (strcmp(ext_prop.extensionName, required_extension) == 0) {
				has_support = true;
				break;
			}
		}

		if (!has_support)
			break;
	}

	return has_support;
}

bool VulkanPhysicalDevice::has_complete_queue_families(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
	auto [graphics_family, present_family] = retrieve_queue_families(physical_device, surface);

	return graphics_family.index != -1 && present_family.index != -1;
}

bool VulkanPhysicalDevice::has_surface_support(VkPhysicalDevice physical_device, uint32_t queue, VkSurfaceKHR surface) {
	VkBool32 has_support = false;

	vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue, surface, &has_support);

	return has_support;
}

void VulkanPhysicalDevice::save_physical_device(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
	m_physical_device = physical_device;

	vkGetPhysicalDeviceProperties(m_physical_device, &m_properties);
	vkGetPhysicalDeviceMemoryProperties(m_physical_device, &m_memory_properties);
	vkGetPhysicalDeviceFeatures(m_physical_device, &m_features);

	auto [graphics_family, present_family] = retrieve_queue_families(physical_device, surface);

	m_graphics_family = graphics_family;
	m_present_family = present_family;
}

std::pair<QueueFamily, QueueFamily> VulkanPhysicalDevice::retrieve_queue_families(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
	uint32_t family_properties_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_properties_count, nullptr);
	std::vector<VkQueueFamilyProperties> family_properties(family_properties_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_properties_count, family_properties.data());

	QueueFamily graphics_family;
	QueueFamily present_family;

	uint32_t queue = 0;
	for (auto& family : family_properties) {
		if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphics_family.index = queue;
			graphics_family.count = family.queueCount;
		}

		if (has_surface_support(physical_device, queue, surface)) {
			present_family.index = queue;
			present_family.count = family.queueCount;
		}

		if (graphics_family.index != std::numeric_limits<uint32_t>::max() &&
			present_family.index != std::numeric_limits<uint32_t>::max())
			return { graphics_family, present_family };

		queue++;
	}

	return std::make_pair<QueueFamily, QueueFamily>({}, {});
}