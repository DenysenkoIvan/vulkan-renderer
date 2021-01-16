#include "VulkanContext.h"

#include <array>
// TODO: Delete this line
#include <iostream>
#include <stdexcept>

void VulkanContext::create(GLFWwindow* window) {
	init_extensions();
	create_instance();

#ifdef VULKAN_DEBUG
	create_debug_messenger();
#endif

	create_surface(window);
	pick_physical_device();
	create_device();
	create_swapchain();
	create_command_pool();
	allocate_command_buffers();
	
	start_rendering();
}

void VulkanContext::destroy() {
	stop_rendering();
	
	vkDestroyCommandPool(m_device, m_command_pool, nullptr);

	for (VkImageView view : m_swapchain_image_views)
		vkDestroyImageView(m_device, view, nullptr);
	
	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	vkDestroyDevice(m_device, nullptr);

#ifdef VULKAN_DEBUG
	auto destroy_debug_messenger_fun = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
	if (!destroy_debug_messenger_fun)
		throw std::runtime_error("Failed to retrieve function pointer to vkDestroyDebugUtilsMessengerEXT");
	else
		destroy_debug_messenger_fun(m_instance, m_debug_messenger, nullptr);
#endif

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}

void VulkanContext::resize(uint32_t width, uint32_t height) {
	stop_rendering();

	VkSurfaceCapabilitiesKHR surface_capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &surface_capabilities);

	m_swapchain_extent = { width, height };

	uint32_t queue_indices[2] = {
		m_graphics_queue_index,
		m_present_queue_index
	};

	VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
	VkSwapchainCreateInfoKHR swapchain_info{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = m_surface,
		.minImageCount = m_image_count,
		.imageFormat = m_surface_format.format,
		.imageColorSpace = m_surface_format.colorSpace,
		.imageExtent = { width, height },
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.pQueueFamilyIndices = queue_indices,
		.preTransform = surface_capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = m_present_mode,
		.clipped = VK_TRUE,
		.oldSwapchain = m_swapchain
	};

	if (m_graphics_queue == m_present_queue) {
		swapchain_info.queueFamilyIndexCount = 1;
		swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	} else {
		swapchain_info.queueFamilyIndexCount = 2;
		swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	}
	
	if (vkCreateSwapchainKHR(m_device, &swapchain_info, nullptr, &new_swapchain) != VK_SUCCESS)
		throw std::runtime_error("Failed to resize swapchain");

	for (VkImageView view : m_swapchain_image_views)
		vkDestroyImageView(m_device, view, nullptr);

	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

	m_swapchain = new_swapchain;

	uint32_t image_count = 0;
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
	m_swapchain_images.resize(image_count);
	m_swapchain_image_views.resize(image_count);

	vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_swapchain_images.data());

	VkImageViewCreateInfo view_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = m_surface_format.format,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	for (uint32_t i = 0; i < image_count; i++) {
		view_info.image = m_swapchain_images[i];

		if (vkCreateImageView(m_device, &view_info, nullptr, &m_swapchain_image_views[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create image view for swapchain image");
	}

	start_rendering();
}

void VulkanContext::swap_buffers() {
	end_buffers();

	submit_command_buffers();
	
	present_image();

	m_frame_index = (m_frame_index + 1) % FRAMES_IN_FLIGHT;

	vkWaitForFences(m_device, 1, &m_frames[m_frame_index].draw_complete_fence, VK_TRUE, UINT64_MAX);

	vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_frames[m_frame_index].image_acquired_semaphore, VK_NULL_HANDLE, &m_image_index);
	
	if (m_images_in_flight_fences[m_image_index] != VK_NULL_HANDLE)
		vkWaitForFences(m_device, 1, &m_images_in_flight_fences[m_image_index], VK_TRUE, UINT64_MAX);
	m_images_in_flight_fences[m_image_index] = m_frames[m_frame_index].draw_complete_fence;

	begin_buffers();
}

void VulkanContext::submit_staging_buffer(VkBuffer buffer, VkDeviceMemory memory) {
	m_frames[m_frame_index].staging_buffers.emplace_back(buffer, memory);
}

void VulkanContext::init_extensions() {
	uint32_t extension_count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
	std::vector<VkExtensionProperties> extensions_supported(extension_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions_supported.data());

	for (const VkExtensionProperties& extention : extensions_supported) {
		const char* extension_name = extention.extensionName;
		
		if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME, extension_name))
			m_instance_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#ifdef _WIN32
		if (!strcmp("VK_KHR_win32_surface", extension_name))
			m_instance_extensions.push_back("VK_KHR_win32_surface");
#else
#error OS not supported
#endif
		
#ifdef VULKAN_DEBUG
		if (!strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, extension_name))
			m_instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	}

	m_physical_device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

void VulkanContext::create_instance() {
	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pEngineName = "Koala",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_0
	};

	std::array<const char*, 1> validation_layers = {
		"VK_LAYER_KHRONOS_validation"
	};

	std::array<VkValidationFeatureEnableEXT, 2> enabled_validation_features = {
			VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
			VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT
	};

	VkValidationFeaturesEXT validation_features{
		.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
		.enabledValidationFeatureCount = (uint32_t)enabled_validation_features.size(),
		.pEnabledValidationFeatures = enabled_validation_features.data()
	};

	VkDebugUtilsMessengerCreateInfoEXT debug_messenger{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.pNext = &validation_features,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
		.pfnUserCallback = debug_callback
	};

	VkInstanceCreateInfo instance_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.enabledExtensionCount = (uint32_t)m_instance_extensions.size(),
		.ppEnabledExtensionNames = m_instance_extensions.data()
	};

#ifdef VULKAN_DEBUG
	instance_info.pNext = &debug_messenger;
	instance_info.enabledLayerCount = (uint32_t)validation_layers.size();
	instance_info.ppEnabledLayerNames = validation_layers.data();
#endif

	if (vkCreateInstance(&instance_info, nullptr, &m_instance) != VK_SUCCESS)
		throw std::runtime_error("Instance creation failed");
}

#ifdef VULKAN_DEBUG
void VulkanContext::create_debug_messenger() {
	VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
		.pfnUserCallback = debug_callback
	};

	auto create_debug_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
	if (!create_debug_messenger)
		throw std::runtime_error("Failed to retrieve function pointer to vkCreateDebugUtilsMessengerEXT");
	else {
		if (create_debug_messenger(m_instance, &debug_messenger_info, nullptr, &m_debug_messenger) != VK_SUCCESS)
			throw std::runtime_error("Failed to create debug messenger");
	}
}
#endif

void VulkanContext::create_surface(GLFWwindow* window) {
	if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS)
		throw std::runtime_error("Failed to create VkSurfaceKHR");
}

void VulkanContext::pick_physical_device() {
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

	if (device_count == 0) throw std::runtime_error("Failed to find GPU with Vulkan support");

	auto calculate_priority = [&](VkPhysicalDevice device) -> uint32_t {
		// Check for extensions support
		uint32_t extensions_count = 0;
		vkEnumerateDeviceExtensionProperties(device, "", &extensions_count, nullptr);
		std::vector<VkExtensionProperties> available_extensions(extensions_count);
		vkEnumerateDeviceExtensionProperties(device, "", &extensions_count, available_extensions.data());

		bool has_extensions_support = false;
		for (const char* required_extension : m_physical_device_extensions) {
			has_extensions_support = false;

			for (const VkExtensionProperties& available_extension : available_extensions) {
				if (!strcmp(required_extension, available_extension.extensionName)) {
					has_extensions_support = true;
					break;
				}
			}

			if (!has_extensions_support)
				return 0;
		}

		// Check for queue families and surface support
		uint32_t family_properties_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &family_properties_count, nullptr);
		std::vector<VkQueueFamilyProperties> family_properties(family_properties_count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &family_properties_count, family_properties.data());

		bool has_graphics_queue = false;
		bool has_present_queue = false;
		uint32_t queue_index = 0;
		for (auto& family : family_properties) {
			if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				has_graphics_queue = true;

			VkBool32 has_surface_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, queue_index, m_surface, &has_surface_support);

			if (has_surface_support)
				has_present_queue = true;

			if (has_graphics_queue && has_surface_support)
				break;

			queue_index++;
		}

		if (!has_graphics_queue || !has_present_queue)
			return 0;

		// Calculate priority based ot properties
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);

		uint32_t priority = 0;
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			priority += 1000;
		if (properties.apiVersion == VK_VERSION_1_2)
			priority += 200;
		else if (properties.apiVersion == VK_VERSION_1_1)
			priority += 100;

		return priority;
	};

	uint32_t max_priority = 0;
	VkPhysicalDevice most_suitable_device = VK_NULL_HANDLE;
	for (VkPhysicalDevice device : devices) {
		uint32_t priority = calculate_priority(device);

		if (priority > max_priority)
			most_suitable_device = device;
	}

	m_physical_device = most_suitable_device;

	m_gpu_info = std::make_unique<PhysicalDeviceInfo>();
	vkGetPhysicalDeviceProperties(m_physical_device, &m_gpu_info->properties);
	vkGetPhysicalDeviceMemoryProperties(m_physical_device, &m_gpu_info->memory_properties);
	vkGetPhysicalDeviceFeatures(m_physical_device, &m_gpu_info->features);

	// Retrieve queues indices
	uint32_t family_properties_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &family_properties_count, nullptr);
	std::vector<VkQueueFamilyProperties> family_properties(family_properties_count);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &family_properties_count, family_properties.data());

	uint32_t queue_index = 0;
	uint32_t graphics_queue_index = ~0;
	uint32_t present_queue_index = ~0;
	for (auto& family : family_properties) {
		if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			graphics_queue_index = queue_index;
		
		VkBool32 has_surface_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, queue_index, m_surface, &has_surface_support);

		if (has_surface_support)
			present_queue_index = queue_index;

		if (graphics_queue_index != ~0 && present_queue_index != ~0)
			break;
	}

	m_graphics_queue_index = graphics_queue_index;
	m_present_queue_index = present_queue_index;
}

void VulkanContext::create_device() {
	float priority = 1;

	auto create_queue_create_info = [&priority](uint32_t index) -> VkDeviceQueueCreateInfo {
		return {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = index,
			.queueCount = 1,
			.pQueuePriorities = &priority
		};
	};

	std::array<VkDeviceQueueCreateInfo, 2> queue_infos{
		create_queue_create_info(m_graphics_queue_index),
		create_queue_create_info(m_present_queue_index)
	};

	uint32_t queue_count = (m_graphics_queue_index == m_present_queue_index ? 1 : 2);

	VkPhysicalDeviceFeatures features{
		.samplerAnisotropy = VK_TRUE
	};

	VkDeviceCreateInfo device_info{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = queue_count,
		.pQueueCreateInfos = queue_infos.data(),
		.enabledExtensionCount = (uint32_t)m_physical_device_extensions.size(),
		.ppEnabledExtensionNames = m_physical_device_extensions.data(),
		.pEnabledFeatures = &features
	};

	if (vkCreateDevice(m_physical_device, &device_info, nullptr, &m_device) != VK_SUCCESS)
		throw std::runtime_error("Failed to create Vulkan Device");

	vkGetDeviceQueue(m_device, m_graphics_queue_index, 0, &m_graphics_queue);

	if (m_graphics_queue_index == m_present_queue_index)
		m_present_queue = m_graphics_queue;
	else
		vkGetDeviceQueue(m_device, m_present_queue_index, 0, &m_present_queue);
}

void VulkanContext::create_swapchain() {
	VkSurfaceCapabilitiesKHR surface_capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &surface_capabilities);

	m_swapchain_extent = surface_capabilities.currentExtent;

	constexpr uint32_t desirable_image_count = 3;
	if (surface_capabilities.maxImageCount < desirable_image_count)
		m_image_count = surface_capabilities.maxImageCount;
	else
		m_image_count = desirable_image_count;

	// Choose format
	uint32_t surface_format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &surface_format_count, nullptr);
	std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &surface_format_count, surface_formats.data());

	if (surface_format_count == 0) throw std::runtime_error("No surface formats available");

	VkFormat desirable_format = VK_FORMAT_B8G8R8A8_SRGB;
	VkColorSpaceKHR desirable_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	
	m_surface_format = surface_formats[0];
	for (const VkSurfaceFormatKHR& surface_format : surface_formats) {
		if (surface_format.format == desirable_format && surface_format.colorSpace == desirable_color_space) {
			m_surface_format = surface_format;
			break;
		}
	}

	// Choose present mode
	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_count, nullptr);
	std::vector<VkPresentModeKHR> present_modes(present_mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_count, present_modes.data());

	VkPresentModeKHR desirable_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
	m_present_mode = VK_PRESENT_MODE_FIFO_KHR;
	for (VkPresentModeKHR present_mode : present_modes) {
		if (present_mode == desirable_present_mode) {
			m_present_mode = desirable_present_mode;
			break;
		}
	}

	// Create swapchain
	std::array<uint32_t, 2> queue_indices{
		m_graphics_queue_index,
		m_present_queue_index
	};

	VkSwapchainCreateInfoKHR swapchain_info{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = m_surface,
		.minImageCount = m_image_count,
		.imageFormat = m_surface_format.format,
		.imageColorSpace = m_surface_format.colorSpace,
		.imageExtent = m_swapchain_extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.pQueueFamilyIndices = queue_indices.data(),
		.preTransform = surface_capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = m_present_mode,
		.clipped = VK_TRUE,
		.oldSwapchain = VK_NULL_HANDLE
	};
	
	if (m_graphics_queue_index == m_present_queue_index) {
		swapchain_info.queueFamilyIndexCount = 1;
		swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	} else {
		swapchain_info.queueFamilyIndexCount = 2;
		swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	}

	if (vkCreateSwapchainKHR(m_device, &swapchain_info, nullptr, &m_swapchain) != VK_NULL_HANDLE)
		throw std::runtime_error("Failed to create swapchain");

	uint32_t swapchain_image_count = 0;
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchain_image_count, nullptr);
	m_swapchain_images.resize(swapchain_image_count);
	m_swapchain_image_views.resize(swapchain_image_count);
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchain_image_count, m_swapchain_images.data());

	VkImageViewCreateInfo image_view_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = m_surface_format.format,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	for (uint32_t i = 0; i < swapchain_image_count; i++) {
		image_view_info.image = m_swapchain_images[i];

		if (vkCreateImageView(m_device, &image_view_info, nullptr, &m_swapchain_image_views[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create swapchain image view");
	}
}

void VulkanContext::create_command_pool() {
	VkCommandPoolCreateInfo pool_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_graphics_queue_index
	};

	if (vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create command pool");
}

void VulkanContext::allocate_command_buffers() {
	VkCommandBufferAllocateInfo buffer_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = m_command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
		if (vkAllocateCommandBuffers(m_device, &buffer_info, &m_frames[i].memory_buffer) != VK_SUCCESS ||
			vkAllocateCommandBuffers(m_device, &buffer_info, &m_frames[i].draw_buffer) != VK_SUCCESS)
			throw std::runtime_error("Failed to allocate command buffer");
	}
}

void VulkanContext::start_rendering() {
	VkSemaphoreCreateInfo semaphore_info{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};

	VkFenceCreateInfo fence_info{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_frames[i].image_acquired_semaphore) != VK_SUCCESS ||
			vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_frames[i].memory_complete_semaphore) != VK_SUCCESS ||
			vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_frames[i].draw_complete_semaphore) != VK_SUCCESS ||
			vkCreateFence(m_device, &fence_info, nullptr, &m_frames[i].draw_complete_fence) != VK_SUCCESS)
			throw std::runtime_error("Failed to create sync objects");
	}

	m_images_in_flight_fences.resize(m_swapchain_images.size());

	m_frame_index = 0;
	vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_frames[m_frame_index].image_acquired_semaphore, VK_NULL_HANDLE, &m_image_index);

	begin_buffers();
}

void VulkanContext::stop_rendering() {
	vkDeviceWaitIdle(m_device);

	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
		vkResetCommandBuffer(m_frames[i].memory_buffer, 0);
		vkResetCommandBuffer(m_frames[i].draw_buffer, 0);

		vkDestroySemaphore(m_device, m_frames[i].image_acquired_semaphore, nullptr);
		vkDestroySemaphore(m_device, m_frames[i].memory_complete_semaphore, nullptr);
		vkDestroySemaphore(m_device, m_frames[i].draw_complete_semaphore, nullptr);
		vkDestroyFence(m_device, m_frames[i].draw_complete_fence, nullptr);

		m_frames[i].image_acquired_semaphore = VK_NULL_HANDLE;
		m_frames[i].memory_complete_semaphore = VK_NULL_HANDLE;
		m_frames[i].draw_complete_semaphore = VK_NULL_HANDLE;
		m_frames[i].draw_complete_fence = VK_NULL_HANDLE;
	}
	m_images_in_flight_fences.clear();
}

void VulkanContext::begin_buffers() {
	vkResetCommandBuffer(m_frames[m_frame_index].memory_buffer, 0);
	vkResetCommandBuffer(m_frames[m_frame_index].draw_buffer, 0);

	for (StagingBuffer& staging : m_frames[m_frame_index].staging_buffers) {
		vkDestroyBuffer(m_device, staging.buffer, nullptr);
		vkFreeMemory(m_device, staging.memory, nullptr);
	}
	m_frames[m_frame_index].staging_buffers.clear();

	VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	vkBeginCommandBuffer(m_frames[m_frame_index].memory_buffer, &begin_info);
	vkBeginCommandBuffer(m_frames[m_frame_index].draw_buffer, &begin_info);
}

void VulkanContext::end_buffers() {
	vkEndCommandBuffer(m_frames[m_frame_index].memory_buffer);
	vkEndCommandBuffer(m_frames[m_frame_index].draw_buffer);
}

void VulkanContext::submit_command_buffers() {
	VkPipelineStageFlags wait_stages[2] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	
	VkSemaphore draw_submit_wait_semaphores[2] = {
		m_frames[m_frame_index].memory_complete_semaphore,
		m_frames[m_frame_index].image_acquired_semaphore
	};

	VkSubmitInfo submits[2]{
		// Memory command buffer submit info
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &m_frames[m_frame_index].memory_buffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &m_frames[m_frame_index].memory_complete_semaphore
		},
		// Draw command buffer submit info
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 2,
			.pWaitSemaphores = draw_submit_wait_semaphores,
			.pWaitDstStageMask = wait_stages,
			.commandBufferCount = 1,
			.pCommandBuffers = &m_frames[m_frame_index].draw_buffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &m_frames[m_frame_index].draw_complete_semaphore
		}
	};

	vkResetFences(m_device, 1, &m_frames[m_frame_index].draw_complete_fence);

	vkQueueSubmit(m_graphics_queue, 2, submits, m_frames[m_frame_index].draw_complete_fence);
}

void VulkanContext::present_image() {
	VkPresentInfoKHR present_info{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_frames[m_frame_index].draw_complete_semaphore,
		.swapchainCount = 1,
		.pSwapchains = &m_swapchain,
		.pImageIndices = &m_image_index
	};

	vkQueuePresentKHR(m_present_queue, &present_info);
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	// TODO: Error logging
	std::cout << pCallbackData->pMessage << '\n';

	return VK_FALSE;
}