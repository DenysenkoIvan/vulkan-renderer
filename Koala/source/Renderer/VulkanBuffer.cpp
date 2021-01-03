#include "VulkanBuffer.h"




std::shared_ptr<VulkanContext> VulkanBuffer::s_context;

void VulkanBuffer::set_context(std::shared_ptr<VulkanContext> context) {
	s_context = context;
}

VulkanBuffer::~VulkanBuffer() {
	clear();
}

void VulkanBuffer::create(const void* data, uint64_t size, VkBufferUsageFlags usage) {
	m_buffer_size = size;
	m_usage = usage;

	BufferAllocationInfo allocated = s_context->allocator().allocate_buffer(data, size, usage);

	m_buffer = allocated.buffer;
	m_buffer_memory = allocated.buffer_memory;
}

void VulkanBuffer::update(const void* data, uint64_t size) {
	if (size <= 65536) {
		s_context->allocator().update_buffer(m_buffer, data, size);
	} else {
		clear();
		create(data, size, m_usage);
	}
}

void VulkanBuffer::clear() {
	m_buffer_size = 0;
	vkDestroyBuffer(s_context->device().device(), m_buffer, nullptr);
	vkFreeMemory(s_context->device().device(), m_buffer_memory, nullptr);

	m_buffer = VK_NULL_HANDLE;
	m_buffer_memory = VK_NULL_HANDLE;
}