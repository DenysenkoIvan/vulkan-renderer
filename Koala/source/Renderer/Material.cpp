#include "Material.h"

std::shared_ptr<Material> Material::create(std::shared_ptr<Shader> shader) {
	std::shared_ptr<Material> material = std::make_shared<Material>(shader);

	return material;
}

Material::Material(std::shared_ptr<Shader> shader)
	: m_shader(shader)
{

}