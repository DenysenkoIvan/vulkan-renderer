#pragma once

#include "Common.h"

#include <vulkan/vulkan.h>

#include <memory>

class VertexBuffer {
public:
	static std::shared_ptr<VertexBuffer> create();
	
	~VertexBuffer();

	void set_format(Format format) { m_format = format; }
	void set_input_rate(VertexInputRate input_rate) { m_input_rate = input_rate; }
	void update(const float* vertices, uint32_t count);

private:
	VertexBuffer();

	void destroy();

private:
	Format m_format = Format::RGBA32_SFloat;
	VertexInputRate m_input_rate = VertexInputRate::Vertex;

	VkBuffer m_buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_memory = VK_NULL_HANDLE;
	VkDeviceSize m_memory_size = 0;


};

class IndexBuffer {
public:
	static std::shared_ptr<IndexBuffer> create();
	
	~IndexBuffer();

	void update(const void* data, uint32_t count, IndexType type);

private:
	IndexBuffer();

	void destroy();

private:
	IndexType m_type = IndexType::Uint16;
	VkBuffer m_buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_memory = VK_NULL_HANDLE;
	VkDeviceSize m_memory_size = 0;
};