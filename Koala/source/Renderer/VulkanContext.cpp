#include "VulkanContext.h"
#include <Profile.h>

#include <array>
// TODO: Delete this line
#include <iostream>
#include <stdexcept>

void VulkanContext::create(GLFWwindow* window) {
	MY_PROFILE_FUNCTION();

	init_extensions();
	create_instance();

#ifdef VULKAN_DEBUG
	create_debug_messenger();
#endif

	create_surface(window);
	pick_physical_device();
	create_device();
	create_swapchain();

	start_rendering();
}

void VulkanContext::destroy() {
	MY_PROFILE_FUNCTION();

	stop_rendering();

	cleanup_swapchain();
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
	MY_PROFILE_FUNCTION();

	stop_rendering();

	cleanup_swapchain();

	create_swapchain();

	start_rendering();
}

void VulkanContext::sync() {
	vkDeviceWaitIdle(m_device);
}

void VulkanContext::swap_buffers(VkCommandBuffer setup_buffer, VkCommandBuffer draw_buffer) {
	MY_PROFILE_FUNCTION();

	// Submit command buffers
	VkSubmitInfo setup_submit{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &setup_buffer
	};

	VkPipelineStageFlags wait_stages[1] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo draw_submit{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_image_acquired_semaphores[m_frame_index],
		.pWaitDstStageMask = wait_stages,
		.commandBufferCount = 1,
		.pCommandBuffers = &draw_buffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_draw_complete_semaphores[m_frame_index]
	};

	VkSubmitInfo submits[2] = { setup_submit, draw_submit };

	{
		MY_PROFILE_SCOPE("Submitting graphics commands");

		vkResetFences(m_device, 1, &m_draw_complete_fences[m_frame_index]);
		vkQueueSubmit(m_graphics_queue, 2, submits, m_draw_complete_fences[m_frame_index]);
	}

	// Present image
	VkPresentInfoKHR present_info{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_draw_complete_semaphores[m_frame_index],
		.swapchainCount = 1,
		.pSwapchains = &m_swapchain,
		.pImageIndices = &m_image_index
	};

	{
		MY_PROFILE_SCOPE("Submitting to present queue");

		vkQueuePresentKHR(m_present_queue, &present_info);
	}

	// Prepare new image
	m_frame_index = (m_frame_index + 1) % FRAMES_IN_FLIGHT;

	prepare_rendering();
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
		.apiVersion = VK_API_VERSION_1_1
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

	if (m_physical_device == VK_NULL_HANDLE)
		throw std::runtime_error("Failed to find suitable physical device");

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
		if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT && family.timestampValidBits)
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
		.wideLines = VK_TRUE,
		.samplerAnisotropy = VK_TRUE
	};

	VkPhysicalDeviceHostQueryResetFeatures host_query_reset_feature{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES,
		.hostQueryReset = VK_TRUE
	};

	VkDeviceCreateInfo device_info{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &host_query_reset_feature,
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
	MY_PROFILE_FUNCTION();

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

	//VkFormat desirable_format = VK_FORMAT_B8G8R8A8_SRGB;
	VkFormat desirable_format = VK_FORMAT_R8G8B8A8_UNORM;
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

	// Retrieve swapchain images
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

	// Create swapchain render pass
	VkAttachmentDescription image_attachment{
		.format = m_surface_format.format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	VkAttachmentReference image_ref{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpass{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &image_ref
	};

	VkRenderPassCreateInfo render_pass_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &image_attachment,
		.subpassCount = 1,
		.pSubpasses = &subpass
	};

	if (vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_swapchain_render_pass) != VK_SUCCESS)
		throw std::runtime_error("Failed to create swapchain render pass");

	// Create swapchain framebuffers
	VkFramebufferCreateInfo framebuffer_info{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = m_swapchain_render_pass,
		.attachmentCount = 1,
		.width = m_swapchain_extent.width,
		.height = m_swapchain_extent.height,
		.layers = 1
	};

	m_swapchain_framebuffers.resize(swapchain_image_count);
	for (uint32_t i = 0; i < swapchain_image_count; i++) {
		framebuffer_info.pAttachments = &m_swapchain_image_views[i];

		if (vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_swapchain_framebuffers[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create swapchain framebuffer");
	}
}

void VulkanContext::cleanup_swapchain() {
	MY_PROFILE_FUNCTION();

	for (VkFramebuffer framebuffer : m_swapchain_framebuffers)
		vkDestroyFramebuffer(m_device, framebuffer, nullptr);
	m_swapchain_framebuffers.clear();

	vkDestroyRenderPass(m_device, m_swapchain_render_pass, nullptr);

	for (VkImageView image_view : m_swapchain_image_views)
		vkDestroyImageView(m_device, image_view, nullptr);
	m_swapchain_image_views.clear();
	m_swapchain_images.clear();

	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}

void VulkanContext::start_rendering() {
	MY_PROFILE_FUNCTION();

	VkSemaphoreCreateInfo semaphore_info{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};

	VkFenceCreateInfo fence_info{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_image_acquired_semaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_draw_complete_semaphores[i]) != VK_SUCCESS ||
			vkCreateFence(m_device, &fence_info, nullptr, &m_draw_complete_fences[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create sync objects");
	}

	m_frame_index = 0;

	prepare_rendering();
}

void VulkanContext::prepare_rendering() {
	MY_PROFILE_FUNCTION();

	{
		MY_PROFILE_SCOPE("Waiting for fences");

		vkWaitForFences(m_device, 1, &m_draw_complete_fences[m_frame_index], VK_TRUE, UINT64_MAX);
	}
	vkResetFences(m_device, 1, &m_draw_complete_fences[m_frame_index]);

	vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_image_acquired_semaphores[m_frame_index], VK_NULL_HANDLE, &m_image_index);
}

void VulkanContext::stop_rendering() {
	MY_PROFILE_FUNCTION();

	vkDeviceWaitIdle(m_device);

	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(m_device, m_image_acquired_semaphores[i], nullptr);
		vkDestroySemaphore(m_device, m_draw_complete_semaphores[i], nullptr);
		vkDestroyFence(m_device, m_draw_complete_fences[i], nullptr);

		m_image_acquired_semaphores[i] = VK_NULL_HANDLE;
		m_draw_complete_semaphores[i] = VK_NULL_HANDLE;
		m_draw_complete_fences[i] = VK_NULL_HANDLE;
	}
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {
	// TODO: Error logging
	std::cout << pCallbackData->pMessage << '\n';

	return VK_FALSE;
}