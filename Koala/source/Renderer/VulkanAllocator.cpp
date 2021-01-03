#include "VulkanAllocator.h"

#include <stdexcept>

void VulkanAllocator::create(std::shared_ptr<VulkanPhysicalDevice> physical_device, std::shared_ptr<VulkanDevice> device) {
	m_physical_device = physical_device;
	m_device = device;
}

void VulkanAllocator::set_submit_memory_commands_callback(const std::function<void(const std::function<void(VkCommandBuffer)>&)>& callback) {
	m_submit_memory_commands_fun = callback;
}

void VulkanAllocator::free_staging_buffers() {
	for (auto& staging_buffer : m_staging_buffers) {
		vkDestroyBuffer(m_device->device(), staging_buffer.buffer, nullptr);
		vkFreeMemory(m_device->device(), staging_buffer.memory, nullptr);
	}

	m_staging_buffers.resize(0);
}

BufferAllocationInfo VulkanAllocator::allocate_buffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage) {
	VkBuffer staging_buffer = create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	VkDeviceMemory staging_memory = allocate_buffer_memory(staging_buffer, size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* staging_data = nullptr;
	vkMapMemory(m_device->device(), staging_memory, 0, size, 0, &staging_data);
	memcpy(staging_data, data, size);
	vkUnmapMemory(m_device->device(), staging_memory);

	VkBuffer buffer = create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage);
	VkDeviceMemory buffer_memory = allocate_buffer_memory(buffer, size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkBufferCopy copy_region{
		.size = size
	};

	m_submit_memory_commands_fun([&](VkCommandBuffer command_buffer) {
		vkCmdCopyBuffer(command_buffer, staging_buffer, buffer, 1, &copy_region);
	});

	m_staging_buffers.emplace_back(staging_buffer, staging_memory);

	return { buffer, buffer_memory };
}

ImageAllocationInfo VulkanAllocator::allocate_image(const void* data, uint32_t size, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, VkImageAspectFlags aspect) {
	VkBuffer staging_buffer = create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	VkDeviceMemory staging_memory = allocate_buffer_memory(staging_buffer, size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* staging_data = nullptr;
	vkMapMemory(m_device->device(), staging_memory, 0, size, 0, &staging_data);
	memcpy(staging_data, data, size);
	vkUnmapMemory(m_device->device(), staging_memory);

	VkImage image = create_image(extent, format, usage);
	VkDeviceMemory image_memory = allocate_image_memory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	transition_image_layout(image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	copy_buffer_to_image(staging_buffer, image, extent, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	transition_image_layout(image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout);

	VkImageView image_view = create_image_view(image, format, aspect);

	return { image, image_memory, image_view };
}

uint32_t VulkanAllocator::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
	const VkPhysicalDeviceMemoryProperties& mem_props = m_physical_device->memory_properties();

	for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
		if ((type_filter & (1 << i)) &&
			(mem_props.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to find appropriate memory type");
}

VkBuffer VulkanAllocator::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage) const {
	VkBufferCreateInfo buffer_info{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VkBuffer buffer;
	if (vkCreateBuffer(m_device->device(), &buffer_info, nullptr, &buffer) != VK_SUCCESS)
		throw std::runtime_error("Failed to create buffer");

	return buffer;
}

VkDeviceMemory VulkanAllocator::allocate_buffer_memory(VkBuffer buffer, VkDeviceSize size, VkMemoryPropertyFlags properties) const {
	VkMemoryRequirements mem_reqs;
	vkGetBufferMemoryRequirements(m_device->device(), buffer, &mem_reqs);

	VkMemoryAllocateInfo memory_info{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, properties)
	};

	VkDeviceMemory memory;
	if (vkAllocateMemory(m_device->device(), &memory_info, nullptr, &memory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate memory");
	
	vkBindBufferMemory(m_device->device(), buffer, memory, 0);

	return memory;
}

ImageAllocationInfo VulkanAllocator::allocate_empty_image(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, VkImageAspectFlags aspect) {
	VkImage image = create_image(extent, format, usage);
	VkDeviceMemory image_memory = allocate_image_memory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkImageView image_view = create_image_view(image, format, aspect);

	return { image, image_memory, image_view };
}

VkImage VulkanAllocator::create_image(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage) const {
	VkImageCreateInfo image_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = { extent.width, extent.height, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	
	VkImage image;
	if (vkCreateImage(m_device->device(), &image_info, nullptr, &image) != VK_SUCCESS)
		throw std::runtime_error("Failed to create image");

	return image;
}

VkDeviceMemory VulkanAllocator::allocate_image_memory(VkImage image, VkMemoryPropertyFlags mem_properties) const {
	VkMemoryRequirements mem_reqs;
	vkGetImageMemoryRequirements(m_device->device(), image, &mem_reqs);

	VkMemoryAllocateInfo memory_info{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, mem_properties)
	};

	VkDeviceMemory memory;
	if (vkAllocateMemory(m_device->device(), &memory_info, nullptr, &memory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate memory");

	vkBindImageMemory(m_device->device(), image, memory, 0);

	return memory;
}

VkImageView VulkanAllocator::create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect) const {
	VkImageViewCreateInfo view_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange = { aspect, 0, 1, 0, 1 }
	};
	
	VkImageView image_view;
	if (vkCreateImageView(m_device->device(), &view_info, nullptr, &image_view) != VK_SUCCESS)
		throw std::runtime_error("Failed to create image view");

	return image_view;
}

void VulkanAllocator::transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) const {
	VkImageMemoryBarrier barrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		.oldLayout = old_layout,
		.newLayout = new_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};
	
	m_submit_memory_commands_fun([&](VkCommandBuffer cmd_buffer) {
		vkCmdPipelineBarrier(
			cmd_buffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	});
}

void VulkanAllocator::copy_buffer_to_image(VkBuffer buffer, VkImage image, VkExtent2D extent, VkImageLayout layout) const {
	VkBufferImageCopy copy{
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.imageOffset = 0,
		.imageExtent = { extent.width, extent.height, 1 }
	};

	m_submit_memory_commands_fun([&](VkCommandBuffer cmd_buffer) {
		vkCmdCopyBufferToImage(cmd_buffer, buffer, image, layout, 1, &copy);
	});
}