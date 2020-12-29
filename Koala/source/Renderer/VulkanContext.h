#pragma once

#include "Common.h"
#include "VulkanDevice.h"
#include "VulkanPhysicalDevice.h"
#include "VulkanSwapchain.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <functional>
#include <optional>
#include <stdexcept>

class VulkanContext {
public:
	void create(GLFWwindow* window);
	~VulkanContext();

	VulkanDevice& device() { return *m_device; }
	VulkanSwapchain& swapchain() { return m_swapchain; }

	void begin_frame();
	void end_frame();

	void submit(const std::function<void(VkCommandBuffer)>& buffer);

	uint32_t image_index() const { return m_image_index; }

	void on_resize(uint32_t width, uint32_t height);

private:
	void create_instance();
	void create_debug_messenger();
	void create_surface(GLFWwindow* window);
	void pick_physical_device();
	void create_device();
	void create_swapchain();
	void create_command_pool();
	void allocate_command_buffers();
	void create_sync_objects();
	void begin_new_render_buffer();
	void submit_command_buffer();

	void begin_rendering();
	void end_rendering();

	void destroy_debug_messenger();
	void destroy_surface();
	void destroy_swapchain();
	void destroy_command_pool();
	void destroy_sync_objects();

	static const std::vector<const char*>& get_required_extensions();

	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

private:
	VkInstance m_instance = VK_NULL_HANDLE;

	VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;

	VkSurfaceKHR m_surface = VK_NULL_HANDLE;

	std::shared_ptr<VulkanPhysicalDevice> m_physical_device;
	std::shared_ptr<VulkanDevice> m_device;

	VulkanSwapchain m_swapchain;
	
	VkCommandPool m_command_pool;
	std::vector<VkCommandBuffer> m_render_buffers;
	
	bool m_should_resize = false;

	uint32_t m_frames_in_flight = 0;
	uint32_t m_image_index = ~0;
	uint32_t m_current_frame = 0;
	std::vector<VkSemaphore> m_image_available_semaphores;
	std::vector<VkSemaphore> m_render_finished_semaphores;
	std::vector<VkFence> m_in_flight_fences;
	std::vector<VkFence> m_images_in_flight;
};