#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

enum class UniformType {
	Sampler = 0,
	CombinedImageSampler,
	SampledImage,
	StorageImage,
	UniformTexelBuffer,
	StorageTexelBuffer,
	UniformBuffer,
	StorageBuffer,
	UniformBufferDynamic,
	StorageBufferDynamic,
	InputAttachment
};

enum class ShaderStage {
	Vertex = 1,
	Fragment = 16
};

struct UniformInfo {
	std::string name;
	UniformType type;
	ShaderStage stage;
	uint32_t binding;
	uint32_t count;

	bool operator<(const UniformInfo& ui) const {
		return binding < ui.binding;
	}

	bool operator==(const UniformInfo& ui) const {
		return name == ui.name && type == ui.type && binding == ui.binding && count == ui.count;
	}
};

struct UniformSetInfo {
	uint32_t set;
	std::vector<UniformInfo> uniforms;

	bool operator<(const UniformSetInfo& usi) const {
		return set < usi.set && uniforms < usi.uniforms;
	}

	bool operator==(const UniformSetInfo& usi) const {
		return set == usi.set && uniforms == usi.uniforms;
	}
};