#include "VulkanBuffer.h"




std::shared_ptr<VulkanContext> VulkanBuffer::s_context;

void VulkanBuffer::set_context(std::shared_ptr<VulkanContext> context) {
	s_context = context;
}

VulkanBuffer::VulkanBuffer(const void* data, uint64_t size, VkBufferUsageFlags usage)
	: m_buffer_size(size)
{
	BufferAllocationInfo allocated = s_context->allocator().allocate_buffer(data, size, usage);

	m_buffer = allocated.buffer;
	m_buffer_memory = allocated.buffer_memory;
}

VulkanBuffer::~VulkanBuffer() {
	clear();
}

void VulkanBuffer::clear() {
	m_buffer_size = 0;
	vkDestroyBuffer(s_context->device().device(), m_buffer, nullptr);
	vkFreeMemory(s_context->device().device(), m_buffer_memory, nullptr);
}