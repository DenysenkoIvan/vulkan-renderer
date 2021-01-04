#include "VulkanSwapchain.h"

#include <array>
#include <stdexcept>

static const uint32_t FRAMES_IN_FLIGHT = 2;

void VulkanSwapchain::create(std::shared_ptr<VulkanPhysicalDevice> physical_device, VkSurfaceKHR surface, std::shared_ptr<VulkanDevice> device) {
	m_surface = surface;
	m_physical_device = physical_device;
	m_device = device;

	retrieve_surface_properties();

	choose_swapchain_extent();
	choose_surface_format();
	choose_present_mode();

	create_swapchain();

	retrieve_images();
}

void VulkanSwapchain::destroy() {
	destroy_image_views();

	vkDestroySwapchainKHR(m_device->device(), m_swapchain, nullptr);
}

uint32_t VulkanSwapchain::acquire_next_image(VkSemaphore signal_semaphore) {
	vkAcquireNextImageKHR(m_device->device(), m_swapchain, UINT64_MAX, signal_semaphore, VK_NULL_HANDLE, &m_image_index);

	return m_image_index;
}

void VulkanSwapchain::present(VkSemaphore wait_semaphore) {
	VkPresentInfoKHR present_info{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &wait_semaphore,
		.swapchainCount = 1,
		.pSwapchains = &m_swapchain,
		.pImageIndices = &m_image_index
	};

	vkQueuePresentKHR(m_device->present_queue(), &present_info);
}

void VulkanSwapchain::on_resize(uint32_t width, uint32_t height) {
	m_image_extent = { width, height };
	
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device->physical_device(), m_surface, &m_surface_capabilities);

	VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
	
	std::array<uint32_t, 2> queues{
		m_physical_device->graphics_family().index,
		m_physical_device->present_family().index
	};

	VkSwapchainCreateInfoKHR swapchain_info{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = m_surface,
		.minImageCount = choose_image_count(),
		.imageFormat = m_surface_format.format,
		.imageColorSpace = m_surface_format.colorSpace,
		.imageExtent = m_image_extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.pQueueFamilyIndices = queues.data(),
		.preTransform = m_surface_capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = m_present_mode,
		.clipped = VK_TRUE,
		.oldSwapchain = m_swapchain
	};

	if (queues[0] == queues[1]) {
		swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_info.queueFamilyIndexCount = 1;
	} else {
		swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_info.queueFamilyIndexCount = 2;
	}

	if (vkCreateSwapchainKHR(m_device->device(), &swapchain_info, nullptr, &new_swapchain) != VK_SUCCESS)
		throw std::runtime_error("Failed to recreate swapchain");
	
	destroy();

	m_swapchain = new_swapchain;

	retrieve_images();
}

void VulkanSwapchain::retrieve_surface_properties() {
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device->physical_device(), m_surface, &m_surface_capabilities);

	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device->physical_device(), m_surface, &format_count, nullptr);

	if (format_count == 0) throw std::runtime_error("No surface formats available");

	m_surface_formats.resize(format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device->physical_device(), m_surface, &format_count, m_surface_formats.data());

	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device->physical_device(), m_surface, &present_mode_count, nullptr);

	if (present_mode_count == 0) throw std::runtime_error("No present modes available");

	m_present_modes.resize(present_mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device->physical_device(), m_surface, &present_mode_count, m_present_modes.data());
}

uint32_t VulkanSwapchain::choose_image_count() {	
	if (m_surface_capabilities.maxImageCount < 3)
		return m_surface_capabilities.maxImageCount;
	else
		return 3;
}

void VulkanSwapchain::choose_surface_format() {
	for (VkSurfaceFormatKHR surface_format : m_surface_formats) {
		if (surface_format.format == VK_FORMAT_B8G8R8_SRGB && surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			m_surface_format = surface_format;
			return;
		}
	}

	m_surface_format = m_surface_formats[0];
}

void VulkanSwapchain::choose_present_mode() {
	if (std::find(m_present_modes.begin(), m_present_modes.end(), VK_PRESENT_MODE_FIFO_RELAXED_KHR) != m_present_modes.end())
		//m_present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR; // For some reason it enables VSync
		m_present_mode = VK_PRESENT_MODE_MAILBOX_KHR; // VSync does not get enabled
	else
		m_present_mode = VK_PRESENT_MODE_FIFO_KHR;
}

void VulkanSwapchain::choose_swapchain_extent() {
	m_image_extent = m_surface_capabilities.currentExtent;
}

void VulkanSwapchain::create_swapchain() {
	std::array<uint32_t, 2> queues{
		m_physical_device->graphics_family().index,
		m_physical_device->present_family().index
	};

	VkSwapchainCreateInfoKHR swapchain_info{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = m_surface,
		.minImageCount = choose_image_count(),
		.imageFormat = m_surface_format.format,
		.imageColorSpace = m_surface_format.colorSpace,
		.imageExtent = m_image_extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = (queues[0] == queues[1] ? 1u : 2u),
		.pQueueFamilyIndices = queues.data(),
		.preTransform = m_surface_capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = m_present_mode,
		.clipped = VK_TRUE,
		.oldSwapchain = VK_NULL_HANDLE
	};

	if (vkCreateSwapchainKHR(m_device->device(), &swapchain_info, nullptr, &m_swapchain) != VK_SUCCESS)
		throw std::runtime_error("Failed to create Swapchain");
}

void VulkanSwapchain::retrieve_images() {
	uint32_t image_count = 0;
	vkGetSwapchainImagesKHR(m_device->device(), m_swapchain, &image_count, nullptr);

	m_images.resize(image_count);
	vkGetSwapchainImagesKHR(m_device->device(), m_swapchain, &image_count, m_images.data());

	m_image_views.resize(image_count);
	for (uint32_t i = 0; i < image_count; i++) {
		VkImageViewCreateInfo image_view_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = m_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = m_surface_format.format,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		if (vkCreateImageView(m_device->device(), &image_view_info, nullptr, &m_image_views[i]) != VK_SUCCESS)
			throw std::runtime_error("Failed to create swapchain image view");
	}
}

void VulkanSwapchain::destroy_image_views() {
	for (auto image_view : m_image_views)
		vkDestroyImageView(m_device->device(), image_view, nullptr);

	m_image_views.resize(0);
	m_images.resize(0);
}