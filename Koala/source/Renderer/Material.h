#pragma once

#include "Shader.h"
#include "Texture.h"

#include <memory>

class Material {
public:
	Material(std::shared_ptr<Shader> shader);

	//void bind_sampler(std::string_view name)
	//void bind_texture_sampler(std::string_view name);

	static std::shared_ptr<Material> create(std::shared_ptr<Shader> shader);

private:
	std::shared_ptr<Shader> m_shader;
};