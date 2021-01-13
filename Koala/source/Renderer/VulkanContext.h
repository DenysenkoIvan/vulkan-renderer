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
	void swap_buffers();

	VkCommandBuffer memory_command_buffer() const { return m_frames[m_frame_index].memory_buffer; }
	VkCommandBuffer draw_command_buffer() const { return m_frames[m_frame_index].draw_buffer; }

	void submit_staging_buffer(VkBuffer buffer, VkDeviceMemory memory);

	VkInstance instance() const { return m_instance; }
	VkPhysicalDevice physical_device() const { return m_physical_device; }
	VkDevice device() const { return m_device; }

	const VkPhysicalDeviceProperties& physical_device_props() const { return m_gpu_info->properties; }
	const VkPhysicalDeviceMemoryProperties physical_device_mem_props() const { return m_gpu_info->memory_properties; }

	uint32_t image_index() const { return m_image_index; }
	VkExtent2D swapchain_extent() const { return m_swapchain_extent; }
	VkFormat swapchain_format() const { return m_surface_format.format; }
	uint32_t swapchain_image_count() const { return (uint32_t)m_swapchain_images.size(); }
	const std::vector<VkImageView>& swapchain_image_views() const { return m_swapchain_image_views; }

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
	void create_command_pool();
	void allocate_command_buffers();
	void start_rendering();
	void stop_rendering();
	
	void begin_buffers();
	void end_buffers();
	void submit_command_buffers();
	void present_image();
	
	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

private:
	struct PhysicalDeviceInfo {
		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceMemoryProperties memory_properties;
		VkPhysicalDeviceFeatures features;
	};

	struct StagingBuffer {
		VkBuffer buffer;
		VkDeviceMemory memory;
	};

	struct Frame {
		VkCommandBuffer memory_buffer;
		VkCommandBuffer draw_buffer;
		VkSemaphore image_acquired_semaphore;
		VkSemaphore memory_complete_semaphore;
		VkSemaphore draw_complete_semaphore;
		VkFence draw_complete_fence;
		std::vector<StagingBuffer> staging_buffers;
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

	uint32_t m_image_index;
	VkSwapchainKHR m_swapchain;
	uint32_t m_image_count;
	VkExtent2D m_swapchain_extent;
	VkSurfaceFormatKHR m_surface_format;
	VkPresentModeKHR m_present_mode;
	std::vector<VkImage> m_swapchain_images;
	std::vector<VkImageView> m_swapchain_image_views;

	VkCommandPool m_command_pool;
	
	uint32_t m_frame_index;
	std::array<Frame, FRAMES_IN_FLIGHT> m_frames;
	//std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> m_memory_buffers;
	//std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> m_draw_buffers;

	//std::array<VkSemaphore, FRAMES_IN_FLIGHT> m_image_acquired_semaphores;
	//std::array<VkSemaphore, FRAMES_IN_FLIGHT> m_memory_complete_semaphores;
	//std::array<VkSemaphore, FRAMES_IN_FLIGHT> m_draw_complete_semaphores;
	//std::array<VkFence, FRAMES_IN_FLIGHT> m_draw_complete_fences;
	std::vector<VkFence> m_images_in_flight_fences;
};