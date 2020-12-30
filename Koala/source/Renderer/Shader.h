#pragma once

class Shader {
public:
	enum class DataType {
		None = 0,
		Float,
		Float2,
		Float3,
		Float4,
		Int,
		Int2,
		Int3,
		Int4,
		Mat3,
		Mat4
	};

private:
};

/*#pragma once

#include "Common.h"
#include "Uniform.h"

#include <vulkan\vulkan.h>
#include <spirv_reflect.h>

#include <filesystem>
#include <vector>

class Shader {
public:
	~Shader();

	static std::shared_ptr<Shader> create(const std::filesystem::path& vertex_spv_file, const std::filesystem::path& fragment_spv_file);

private:
	Shader(const std::filesystem::path& vertex_spv_file, const std::filesystem::path& fragment_spv_file);

	std::vector<uint8_t> load_spirv_code(const std::filesystem::path& spv_path);
	void reflect_shader(const std::vector<uint8_t>& spv_code);
	void reflect_vertex_shader(spv_reflect::ShaderModule& shader_module);
	void reflect_fragment_shader(spv_reflect::ShaderModule& shader_module);
	void reflect_descriptor_sets(const std::vector<SpvReflectDescriptorSet*>& sets);
	void reflect_descriptor_binding(std::vector<UniformInfo>& uniforms, SpvReflectDescriptorBinding* binding);
	void create_descriptor_set_layouts();
	void create_pipeline_layout();
	VkPipelineShaderStageCreateInfo create_pipeline_shader_stage_create_info(VkShaderModule shader_module, VkShaderStageFlags stage);

private:
	static std::vector<std::shared_ptr<Shader>> s_shader_library;

	struct InputVariable {
		std::string name;
		Format format;
		uint32_t location;
	};

	struct DescriptorSetInfo {
		UniformSetInfo set_info;
		VkDescriptorSetLayout layout;

		bool operator<(const DescriptorSetInfo& si) const {
			return set_info < si.set_info;
		}

		bool operator==(const DescriptorSetInfo& dsi) const {
			return set_info == dsi.set_info;
		}
	};

	std::filesystem::path m_vert_shader_path;
	std::filesystem::path m_frag_shader_path;
	VkShaderModule m_vertex_module;
	VkShaderModule m_fragment_module;
	VkPipelineShaderStageCreateInfo m_vertex_create_info;
	VkPipelineShaderStageCreateInfo m_fragment_create_info;
	std::vector<DescriptorSetInfo> m_descriptor_sets_info;
	//std::vector<InputVariable>
	VkPipelineLayout m_pipeline_layout;
};*/