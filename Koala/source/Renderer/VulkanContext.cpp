#include "VulkanContext.h"

#include <Core/Window.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>

// TODO: Delete this line
#include <iostream>

#include <limits>
#include <stdexcept>
#include <vector>

#ifndef NDEBUG
#define VULKAN_DEBUG
#endif

void VulkanContext::create(GLFWwindow* window) {
	create_instance();

#if defined(VULKAN_DEBUG)
	create_debug_messenger();
#endif

	create_surface(window);

	pick_physical_device();

	create_device();

	create_allocator();

	create_swapchain();

	create_command_pool();
	allocate_command_buffers();

	begin_rendering();
}

VulkanContext::~VulkanContext() {
	end_rendering();

	destroy_command_pool();

	destroy_swapchain();

	m_device->destroy();

	destroy_surface();

#ifdef VULKAN_DEBUG
	destroy_debug_messenger();
#endif

	vkDestroyInstance(m_instance, nullptr);
}

void VulkanContext::on_resize(uint32_t width, uint32_t height) {
	end_rendering();

	m_swapchain.on_resize(width, height);

	begin_rendering();
}

void VulkanContext::begin_frame() {
	begin_memory_buffer();
	begin_new_render_buffer();
}

void VulkanContext::end_frame() {
	submit_memory_buffer();
	submit_render_buffer();

	m_swapchain.present(m_render_finished_semaphores[m_current_frame], m_image_index);

	m_current_frame = (m_current_frame + 1) % m_frames_in_flight;

	vkWaitForFences(m_device->device(), 1, &m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX);

	m_image_index = m_swapchain.acquire_next_image(m_image_available_semaphores[m_current_frame]);

	if (m_images_in_flight[m_image_index] != VK_NULL_HANDLE)
		vkWaitForFences(m_device->device(), 1, &m_images_in_flight[m_image_index], VK_TRUE, UINT64_MAX);
	m_images_in_flight[m_image_index] = m_in_flight_fences[m_current_frame];

	begin_new_render_buffer();
}

void VulkanContext::submit_render_commands(const std::function<void(VkCommandBuffer)>& submit_fun) {
	submit_fun(m_render_buffers[m_image_index]);
}

void VulkanContext::create_instance() {
	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pEngineName = "Koala",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_0
	};

	uint32_t extensions_count = 0;
	const char** extensions_ptr = glfwGetRequiredInstanceExtensions(&extensions_count);
	std::vector<const char*> extensions(extensions_ptr, extensions_ptr + extensions_count);

#ifdef VULKAN_DEBUG
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

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
		.enabledExtensionCount = (uint32_t)extensions.size(),
		.ppEnabledExtensionNames = extensions.data()
	};

#ifdef VULKAN_DEBUG
	instance_info.pNext = &debug_messenger;
	instance_info.enabledLayerCount = (uint32_t)validation_layers.size();
	instance_info.ppEnabledLayerNames = validation_layers.data();
#endif

	if (vkCreateInstance(&instance_info, nullptr, &m_instance) != VK_SUCCESS)
		throw std::runtime_error("Instance creation failed");
}

void VulkanContext::create_debug_messenger() {
	VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
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

void VulkanContext::create_surface(GLFWwindow* window) {
	if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS)
		throw std::runtime_error("Failed to create VkSurfaceKHR");
}

void VulkanContext::pick_physical_device() {
	m_physical_device = std::make_shared<VulkanPhysicalDevice>();

	if (m_physical_device->pick(m_instance, get_required_extensions(), m_surface) != VK_SUCCESS)
		throw std::runtime_error("Failed to pick GPU");
}

void VulkanContext::create_device() {
	m_device = std::make_shared<VulkanDevice>();

	m_device->create(*m_physical_device, get_required_extensions());
}

void VulkanContext::create_allocator() {
	m_allocator.create(m_physical_device, m_device);
	m_allocator.set_submit_memory_commands_callback([&](const std::function<void(VkCommandBuffer)>& memory_commands) {
		memory_commands(m_memory_buffer);
		m_execute_memory_commands = true;
	});
}

void VulkanContext::create_swapchain() {
	m_swapchain.create(m_physical_device, m_surface, m_device);

	uint32_t image_count = (uint32_t)m_swapchain.images().size();
	
	const uint32_t appropriate_frames_in_flight_count = 2;

	if (image_count <= appropriate_frames_in_flight_count)
		m_frames_in_flight = image_count;
	else
		m_frames_in_flight = appropriate_frames_in_flight_count;
}

void VulkanContext::create_command_pool() {
	VkCommandPoolCreateInfo pool_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_physical_device->graphics_family().index
	};

	if (vkCreateCommandPool(m_device->device(), &pool_info, nullptr, &m_command_pool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create command pool");
}

void VulkanContext::allocate_command_buffers() {
	m_render_buffers.resize(m_swapchain.image_count());

	VkCommandBufferAllocateInfo render_buffer_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = m_command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = (uint32_t)m_render_buffers.size()
	};

	VkCommandBufferAllocateInfo memory_buffer_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = m_command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	vkAllocateCommandBuffers(m_device->device(), &render_buffer_info, m_render_buffers.data());
	vkAllocateCommandBuffers(m_device->device(), &memory_buffer_info, &m_memory_buffer);
}

void VulkanContext::create_sync_objects() {
	m_image_available_semaphores.resize(m_frames_in_flight);
	m_render_finished_semaphores.resize(m_frames_in_flight);
	m_in_flight_fences.resize(m_frames_in_flight);
	m_images_in_flight.resize(m_swapchain.image_count());

	VkSemaphoreCreateInfo semaphore_info{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};

	VkFenceCreateInfo fence_info{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	VkFenceCreateInfo mem_fence_info{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
	};

	if (//vkCreateSemaphore(m_device->device(), &semaphore_info, nullptr, &m_mem_commands_finished_semaphore) != VK_SUCCESS ||
		vkCreateFence(m_device->device(), &mem_fence_info, nullptr, &m_mem_command_fence) != VK_SUCCESS)
		throw std::runtime_error("Failed to create memory commands semaphore");

	for (uint32_t i = 0; i < m_frames_in_flight; i++) {
		if (vkCreateSemaphore(m_device->device(), &semaphore_info, nullptr, &m_image_available_semaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(m_device->device(), &semaphore_info, nullptr, &m_render_finished_semaphores[i]) != VK_SUCCESS ||
			vkCreateFence(m_device->device(), &fence_info, nullptr, &m_in_flight_fences[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create sync objects");
	}
}

void VulkanContext::begin_memory_buffer() {
	vkResetCommandBuffer(m_memory_buffer, 0);

	VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
	};

	if (vkBeginCommandBuffer(m_memory_buffer, &begin_info) != VK_SUCCESS)
		throw std::runtime_error("Failed to begin memory buffer");
}

void VulkanContext::begin_new_render_buffer() {
	vkResetCommandBuffer(m_render_buffers[m_image_index], 0);

	VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
	};

	if (vkBeginCommandBuffer(m_render_buffers[m_image_index], &begin_info) != VK_SUCCESS)
		throw std::runtime_error("Failed to begin command buffer");
}

void VulkanContext::submit_memory_buffer() {
	if (m_execute_memory_commands) {
		m_execute_memory_commands = false;

		vkEndCommandBuffer(m_memory_buffer);

		// Waiting for previous frame to finish rendering in case data it depends on changes suddenly
		// Potential bug if more than 2 frames can be in flight at the moment
		// Should wait on all previous frames in that case
		uint32_t prev_frame = m_current_frame == 0 ? m_frames_in_flight - 1 : m_current_frame - 1;
		vkWaitForFences(m_device->device(), 1, &m_in_flight_fences[prev_frame], VK_TRUE, UINT64_MAX);

		VkSubmitInfo memory_submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &m_memory_buffer,
		};

		vkResetFences(m_device->device(), 1, &m_mem_command_fence);

		// TODO: Delete this line
		std::cout << "Submitting memory commands\n";

		vkQueueSubmit(m_device->graphics_queue(), 1, &memory_submit_info, m_mem_command_fence);

		vkWaitForFences(m_device->device(), 1, &m_mem_command_fence, VK_TRUE, UINT64_MAX);
	}
}

void VulkanContext::submit_render_buffer() {
	vkEndCommandBuffer(m_render_buffers[m_image_index]);
	 
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT	};

	VkSemaphore wait_semaphores[] = { m_image_available_semaphores[m_current_frame]	};

	VkSubmitInfo render_submit_info{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = wait_semaphores,
		.pWaitDstStageMask = wait_stages,
		.commandBufferCount = 1,
		.pCommandBuffers = &m_render_buffers[m_image_index],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_render_finished_semaphores[m_current_frame]
	};

	vkResetFences(m_device->device(), 1, &m_in_flight_fences[m_current_frame]);

	// TODO: Delete this line
	std::cout << "Submitting render commands\n";

	vkQueueSubmit(m_device->graphics_queue(), 1, &render_submit_info, m_in_flight_fences[m_current_frame]);
}

void VulkanContext::begin_rendering() {
	create_sync_objects();

	m_image_index = m_swapchain.acquire_next_image(m_image_available_semaphores[m_current_frame]);

	begin_new_render_buffer();
}

void VulkanContext::end_rendering() {
	vkDeviceWaitIdle(m_device->device());

	for (VkCommandBuffer cb : m_render_buffers)
		vkResetCommandBuffer(cb, 0);

	destroy_sync_objects();
}

void VulkanContext::destroy_debug_messenger() {
	auto destroy_debug_messenger_fun = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
	if (!destroy_debug_messenger_fun)
		throw std::runtime_error("Failed to retrieve function pointer to vkDestroyDebugUtilsMessengerEXT");
	else
		destroy_debug_messenger_fun(m_instance, m_debug_messenger, nullptr);
}

void VulkanContext::destroy_surface() {
	auto destroy_surface_fun = (PFN_vkDestroySurfaceKHR)vkGetInstanceProcAddr(m_instance, "vkDestroySurfaceKHR");
	if (!destroy_surface_fun)
		throw std::runtime_error("Failed to retrieve function pointer vkDestroySurfaceKHR");
	else
		destroy_surface_fun(m_instance, m_surface, nullptr);
}

void VulkanContext::destroy_swapchain() {
	m_swapchain.destroy();
	m_frames_in_flight = 0;
}

void VulkanContext::destroy_command_pool() {
	vkDestroyCommandPool(m_device->device(), m_command_pool, nullptr);
}

void VulkanContext::destroy_sync_objects() {
	for (size_t i = 0; i < m_frames_in_flight; i++) {
		vkDestroySemaphore(m_device->device(), m_image_available_semaphores[i], nullptr);
		vkDestroySemaphore(m_device->device(), m_render_finished_semaphores[i], nullptr);
		vkDestroyFence(m_device->device(), m_in_flight_fences[i], nullptr);
	}
	vkDestroyFence(m_device->device(), m_mem_command_fence, nullptr);

	m_image_available_semaphores.clear();
	m_render_finished_semaphores.clear();
	m_in_flight_fences.clear();
	m_images_in_flight.clear();
}

const std::vector<const char*>& VulkanContext::get_required_extensions() {
	static std::vector<const char*> extensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	return extensions;
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