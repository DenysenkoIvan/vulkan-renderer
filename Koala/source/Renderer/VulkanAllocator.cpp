#include "VulkanAllocator.h"

#include <stdexcept>

void VulkanAllocator::create(std::shared_ptr<VulkanPhysicalDevice> physical_device, std::shared_ptr<VulkanDevice> device) {
	m_physical_device = physical_device;
	m_device = device;
}

void VulkanAllocator::set_submit_memory_commands_callback(const std::function<void(const std::function<void(VkCommandBuffer)>&)>& callback) {
	m_submit_memory_commands_fun = callback;
}

BufferAllocationInfo VulkanAllocator::allocate_buffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage) const {
	VkBuffer staging_buffer = create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	VkDeviceMemory staging_memory = allocate_memory(staging_buffer, size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* staging_data = nullptr;
	vkMapMemory(m_device->device(), staging_memory, 0, size, 0, &staging_data);
	memcpy(staging_data, data, size);
	vkUnmapMemory(m_device->device(), staging_memory);

	VkBuffer buffer = create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage);
	VkDeviceMemory buffer_memory = allocate_memory(buffer, size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkBufferCopy copy_region{
		.size = size
	};

	m_submit_memory_commands_fun([&](VkCommandBuffer command_buffer) {
		vkCmdCopyBuffer(command_buffer, staging_buffer, buffer, 1, &copy_region);
	});

	//return { staging_buffer, staging_memory };
	return { buffer, buffer_memory, staging_buffer, staging_memory };
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

VkDeviceMemory VulkanAllocator::allocate_memory(VkBuffer buffer, VkDeviceSize new_size, VkMemoryPropertyFlags properties) const {
	VkMemoryRequirements mem_reqs;
	vkGetBufferMemoryRequirements(m_device->device(), buffer, &mem_reqs);

	new_size = mem_reqs.size;

	VkMemoryAllocateInfo memory_info{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, properties)
	};

	VkDeviceMemory memory;
	if (vkAllocateMemory(m_device->device(), &memory_info, nullptr, &memory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate memory");
	
	VkResult res = vkBindBufferMemory(m_device->device(), buffer, memory, 0);

	return memory;
}