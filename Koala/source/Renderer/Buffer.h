#pragma once

#include "Shader.h"
#include "VulkanBuffer.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>

struct BufferElement {
	std::string name;
	Shader::DataType type = Shader::DataType::None;
	uint32_t size = 0;
	uint32_t offset = 0;

	BufferElement(Shader::DataType t, std::string n);
};

enum class InputRate {
	Instance,
	Vertex
};

class BufferLayout {
public:
	BufferLayout(std::initializer_list<BufferElement> elements);
	
	InputRate input_rate() const { return m_input_rate; }
	uint32_t stride() const { return m_stride; }

private:
	void calculate_offsets_and_stride();

private:
	std::vector<BufferElement> m_elements;
	InputRate m_input_rate = InputRate::Vertex;
	uint32_t m_stride = 0;
};

class VertexBuffer {
public:
	VertexBuffer(const void* data, uint64_t size, BufferLayout layout);

	void update(const void* data, uint64_t size);

	const VulkanBuffer& buffer() const { return m_vk_buffer; }

	static std::shared_ptr<VertexBuffer> create(const void* data, uint64_t size, BufferLayout layout);

private:
	BufferLayout m_layout;
	VulkanBuffer m_vk_buffer;
};

class IndexBuffer {
public:
	enum class Type {
		UInt16 = 0,
		UInt32 = 1
	};

	IndexBuffer(const void* data, uint64_t size, Type type);

	Type type() const { return m_type; }
	uint32_t index_count() const { return m_index_count; }

	const VulkanBuffer& buffer() const { return m_vk_buffer; }

	static std::shared_ptr<IndexBuffer> create(const void* data, uint64_t size, Type type);

private:
	Type m_type;
	uint32_t m_index_count = 0;
	VulkanBuffer m_vk_buffer;
};