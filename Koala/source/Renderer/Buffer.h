#pragma once

#include "Common.h"
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

class BufferLayout {
public:
	BufferLayout(std::initializer_list<BufferElement> elements);
	
	uint32_t stride() const { return m_stride; }

private:
	void calculate_offsets_and_stride();

private:
	std::vector<BufferElement> m_elements;
	uint32_t m_stride = 0;
};

enum class BufferState {
	NotReady,
	Ready
};

class VertexBuffer {
public:
	VertexBuffer(const void* data, uint64_t size, BufferLayout layout);

	void update_state(BufferState state);

	static std::shared_ptr<VertexBuffer> create(const void* data, uint64_t size, BufferLayout layout);

private:
	BufferState m_state = BufferState::NotReady;
	BufferLayout m_layout;
	VulkanBuffer m_vk_buffer;
};