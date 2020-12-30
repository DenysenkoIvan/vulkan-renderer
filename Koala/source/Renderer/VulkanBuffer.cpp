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

	m_staging_buffer = allocated.staging_buffer;
	m_staging_memory = allocated.staging_memory;
}

VulkanBuffer::~VulkanBuffer() {
	if (m_staging_buffer)
		release_staging_buffer();

	vkDestroyBuffer(s_context->device().device(), m_buffer, nullptr);
	vkFreeMemory(s_context->device().device(), m_buffer_memory, nullptr);
}

void VulkanBuffer::release_staging_buffer() {
	vkDestroyBuffer(s_context->device().device(), m_staging_buffer, nullptr);
	vkFreeMemory(s_context->device().device(), m_staging_memory, nullptr);

	m_staging_buffer = VK_NULL_HANDLE;
	m_staging_memory = VK_NULL_HANDLE;
}