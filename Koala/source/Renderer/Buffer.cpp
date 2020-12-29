#include "Buffer.h"
#include "VulkanContext.h"

#include <stdexcept>

std::shared_ptr<VertexBuffer> VertexBuffer::create() {
	VertexBuffer* vb = new VertexBuffer();

	return std::shared_ptr<VertexBuffer>(vb);
}

VertexBuffer::~VertexBuffer() {
	destroy();
}

VertexBuffer::VertexBuffer() {
}


void VertexBuffer::update(const float* vertices, uint32_t count) {
	if (m_buffer != VK_NULL_HANDLE)
		destroy();

	//m_buffer = VulkanContext::create_buffer(count * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
}

void VertexBuffer::destroy() {
	//VulkanContext::destroy_buffer(m_buffer);
}




std::shared_ptr<IndexBuffer> IndexBuffer::create() {
	IndexBuffer* ib = new IndexBuffer();

	return std::shared_ptr<IndexBuffer>(ib);
}

IndexBuffer::~IndexBuffer() {
	destroy();
}

IndexBuffer::IndexBuffer() {

}


void IndexBuffer::update(const void* data, uint32_t count, IndexType type) {
	if (m_buffer != VK_NULL_HANDLE)
		destroy();

}

void IndexBuffer::destroy() {

}