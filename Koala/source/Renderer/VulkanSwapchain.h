#pragma once

#include "VulkanPhysicalDevice.h"
#include "VulkanDevice.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

class VulkanSwapchain {
public:
	void create(std::shared_ptr<VulkanPhysicalDevice> physical_device, VkSurfaceKHR surface, std::shared_ptr<VulkanDevice> device);
	void destroy();

	size_t image_count() const { return m_images.size(); }
	VkFormat format() const { return m_surface_format.format; }
	VkExtent2D image_extent() const { return m_image_extent; }

	const std::vector<VkImage>& images() const { return m_images; }
	const std::vector<VkImageView>& image_views() const { return m_image_views; }
	
	uint32_t acquire_next_image(VkSemaphore signal_semaphore);
	void present(VkSemaphore wait_semaphore, uint32_t image_index);

	void on_resize(uint32_t width, uint32_t height);


private:
	void retrieve_surface_properties();
	uint32_t choose_image_count();
	void choose_surface_format();
	void choose_present_mode();
	void choose_swapchain_extent();
	void create_swapchain();
	void retrieve_images();

	void destroy_image_views();

private:
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	size_t m_current_frame = 0;
	uint32_t m_image_index = 0;

	VkSurfaceKHR m_surface;
	std::shared_ptr<VulkanPhysicalDevice> m_physical_device;
	std::shared_ptr<VulkanDevice> m_device;
	
	std::vector<VkImage> m_images;
	std::vector<VkImageView> m_image_views;

	VkSurfaceFormatKHR m_surface_format;
	VkPresentModeKHR m_present_mode;
	VkExtent2D m_image_extent;

	VkSurfaceCapabilitiesKHR m_surface_capabilities;
	std::vector<VkSurfaceFormatKHR> m_surface_formats;
	std::vector<VkPresentModeKHR> m_present_modes;
};