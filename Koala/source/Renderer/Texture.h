#pragma once

#include "VulkanImage.h"

enum class Format {

};

class Texture {
public:
	Texture(const void* data, uint32_t size, uint32_t widht, uint32_t height, Format format);
	

private:
	VulkanImage m_vk_image;
};