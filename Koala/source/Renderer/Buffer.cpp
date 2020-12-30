#include "Buffer.h"
#include "VulkanContext.h"

#include <stdexcept>

static constexpr uint32_t shader_data_type_size(Shader::DataType type) {
	switch (type) {
		case Shader::DataType::Float:  return 4;
		case Shader::DataType::Float2: return 4 * 2;
		case Shader::DataType::Float3: return 4 * 3;
		case Shader::DataType::Float4: return 4 * 4;
		case Shader::DataType::Int:    return 4;
		case Shader::DataType::Int2:   return 4 * 2;
		case Shader::DataType::Int3:   return 4 * 3;
		case Shader::DataType::Int4:   return 4 * 4;
		case Shader::DataType::Mat3:   return 4 * 3 * 3;
		case Shader::DataType::Mat4:   return 4 * 4 * 4;
	}

	throw std::runtime_error("Unknown Shader::DataType");
}

BufferElement::BufferElement(Shader::DataType t, std::string n)
	: name(std::move(n)), type(t)
{
	size = shader_data_type_size(type);
}

BufferLayout::BufferLayout(std::initializer_list<BufferElement> elements)
	: m_elements(elements)
{
	calculate_offsets_and_stride();
}

void BufferLayout::calculate_offsets_and_stride() {
	uint32_t offset = 0;
	for (auto& element : m_elements) {
		element.offset = offset;

		offset += element.size;
	}

	m_stride = offset;
}

VertexBuffer::VertexBuffer(const void* data, uint64_t size, BufferLayout layout)
	: m_layout(std::move(layout)), m_vk_buffer(data, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {}

void VertexBuffer::update_state(BufferState state) {
	m_state = state;
	
	if (state == BufferState::Ready)
		m_vk_buffer.release_staging_buffer();
}

std::shared_ptr<VertexBuffer> VertexBuffer::create(const void* data, uint64_t size, BufferLayout layout) {
	std::shared_ptr<VertexBuffer> buffer = std::make_shared<VertexBuffer>(data, size, std::move(layout));

	return buffer;
}