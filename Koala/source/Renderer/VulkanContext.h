#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <array>
#include <memory>
#include <vector>

#ifndef NDEBUG
#define VULKAN_DEBUG
#endif

static constexpr int FRAMES_IN_FLIGHT = 2;

class VulkanContext {
public:
	void create(GLFWwindow* window);
	void destroy();

	void resize(uint32_t widht, uint32_t height);

	void sync();

	void swap_buffers(VkCommandBuffer setup_buffer, VkCommandBuffer draw_buffer);

	VkInstance instance() const { return m_instance; }
	VkPhysicalDevice physical_device() const { return m_physical_device; }
	VkDevice device() const { return m_device; }
	uint32_t graphics_queue_index() const { return m_graphics_queue_index; }

	const VkPhysicalDeviceProperties& physical_device_props() const { return m_gpu_info->properties; }
	const VkPhysicalDeviceMemoryProperties physical_device_mem_props() const { return m_gpu_info->memory_properties; }

	VkExtent2D swapchain_extent() const { return m_swapchain_extent; }
	VkFormat swapchain_format() const { return m_surface_format.format; }
	uint32_t swapchain_image_count() const { return (uint32_t)m_swapchain_images.size(); }
	VkRenderPass swapchain_render_pass() const { return m_swapchain_render_pass; }
	VkFramebuffer swapchain_framebuffer() const { return m_swapchain_framebuffers[m_image_index]; }

private:
	void init_extensions();
	void create_instance();
#ifdef VULKAN_DEBUG
	void create_debug_messenger();
#endif
	void create_surface(GLFWwindow* window);
	void pick_physical_device();
	void create_device();
	void create_swapchain();
	void cleanup_swapchain();
	void start_rendering();
	void prepare_rendering();
	void stop_rendering();
	
	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

private:
	struct PhysicalDeviceInfo {
		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceMemoryProperties memory_properties;
		VkPhysicalDeviceFeatures features;
	};

	struct SwapchainImageResource {
		VkImage image;
		VkImageView view;
		VkFramebuffer framebuffer;
		VkSemaphore image_acquired_semaphore;
		VkSemaphore draw_complete_semaphore;
		VkFence image_in_use_fence;
	};
	
	std::vector<const char*> m_instance_extensions;
	std::vector<const char*> m_physical_device_extensions;

	VkInstance m_instance;
	
#ifdef VULKAN_DEBUG
	VkDebugUtilsMessengerEXT m_debug_messenger;
#endif

	VkSurfaceKHR m_surface;
	
	VkPhysicalDevice m_physical_device;
	std::unique_ptr<PhysicalDeviceInfo> m_gpu_info;
	
	VkDevice m_device;
	uint32_t m_graphics_queue_index;
	uint32_t m_present_queue_index;
	VkQueue m_graphics_queue;
	VkQueue m_present_queue;

	VkSwapchainKHR m_swapchain;
	uint32_t m_image_count;
	VkExtent2D m_swapchain_extent;
	VkSurfaceFormatKHR m_surface_format;
	VkPresentModeKHR m_present_mode;
	std::vector<VkImage> m_swapchain_images;
	std::vector<VkImageView> m_swapchain_image_views;
	VkRenderPass m_swapchain_render_pass;
	std::vector<VkFramebuffer> m_swapchain_framebuffers;

	uint32_t m_image_index;
	uint32_t m_frame_index;
	std::array<VkSemaphore, FRAMES_IN_FLIGHT> m_image_acquired_semaphores;
	std::array<VkSemaphore, FRAMES_IN_FLIGHT> m_draw_complete_semaphores;
	std::array<VkFence, FRAMES_IN_FLIGHT> m_draw_complete_fences;
};