#pragma once

#include "Common.h"
#include "VulkanContext.h"
#include "VulkanGraphicsController.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tinygltf/tiny_gltf.h>

#include <optional>

using VertexBufferId = RenderId;
using IndexBufferId = RenderId;
using MaterialId = RenderId;
using PrimitiveId = RenderId;
using SkyboxId = RenderId;
using NodeId = RenderId;

enum class MagFilter : uint32_t {
	Nearest,
	Linear
};

enum class MinFilter : uint32_t {
	NearestMipMapNearest,
	LinearMipMapNearest,
	NearestMipMapLinear,
	LinearMipMapLinear
};

enum class Wrap : uint32_t {
	ClampToEdge,
	MirroredRepeat,
	Repeat
};

struct SamplerSpecs {
	MagFilter mag_filter;
	MinFilter min_filter;
	Wrap wrap_u;
	Wrap wrap_v;
};

struct ImageSpecs {
	uint32_t width;
	uint32_t height;
	const void* data;
	Format data_format;
	Format desired_format;
};

struct TextureSpecs {
	uint32_t image_id;
	uint32_t sampler_id;
};

struct Camera {
	glm::vec3 eye = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 front;
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

	float aspect_ratio;
	float near;
	float far;

	void move_forward(float movement) {
		eye += front * movement;
	}

	void move_left(float movement) {
		glm::vec3 left = glm::cross(up, front);
		eye += left * movement;
	}

	void turn_left(float degrees) {
		glm::mat4 m = glm::rotate(glm::mat4(1.0f), glm::radians(degrees), up);
		front = m * glm::vec4(front, 1.0f);
		front = glm::normalize(front);
	}

	void turn_up(float degrees) {
		glm::mat4 m = glm::rotate(glm::mat4(1.0f), glm::radians(degrees), glm::cross(up, front));
		front = m * glm::vec4(front, 1.0f);
		front = glm::normalize(front);
	}

	glm::mat4 view_matrix() const {
		return glm::lookAt(eye, eye + front, up);
	}

	glm::mat4 proj_matrix() const {
		glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect_ratio, near, far);
		proj[1][1] *= -1;

		return proj;
	}
};

enum class LightType : uint8_t {
	Directional = 1,
	Spot = 2
};

struct Light {
	LightType type;
	glm::vec3 color;
	glm::vec3 pos;
	glm::vec3 dir;
};

struct MaterialInfo {
	glm::vec4 base_color_factor = glm::vec4(1.0f);
	glm::vec4 emissive_factor = glm::vec4(0.0f);
	float metallic_factor = 1.0f;
	float roughness_factor = 1.0f;
	int base_color_uv_set = -1;
	int ao_rough_met_uv_set = -1;
	int normals_uv_set = -1;
	int emissive_uv_set = -1;
	float alpha_mask = 0.0f; // 0 - no mask, 1 - mask
	float alpha_cutoff = 0.5f;
	float is_ao_in_rough_met = 0.0f;
};

enum struct AlphaMode : uint32_t {
	Opaque,
	Mask,
	Blend
};

struct MaterialSpecs {
	MaterialInfo info;
	AlphaMode alpha_mode = AlphaMode::Opaque;
	std::optional<uint32_t> albedo_id;
	std::optional<uint32_t> ao_rough_met_id;
	std::optional<uint32_t> normals_id;
	std::optional<uint32_t> emissive_id;
};

struct Vertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec4 tangent;
	glm::vec2 uv0;
	glm::vec2 uv1;
};

class Renderer {
public:
	void create(VulkanContext* context);
	void destroy();

	void set_resolution(uint32_t width, uint32_t height);
	void set_shadow_map_resolution(uint32_t width, uint32_t height);
	void set_post_effect_constants(float exposure, float gamma);

	void begin_frame(const Camera& camera, Light dir_light, Light* lights, uint32_t light_count);
	void end_frame(uint32_t width, uint32_t height);

	void draw_primitive(const glm::mat4& model, size_t vertex_buffer, size_t index_buffer, size_t first_index, size_t index_count, size_t vertex_count, MaterialId material);
	void draw_skybox(SkyboxId skybox_id);

	MaterialId materials_create(ImageSpecs* images, uint32_t image_count, SamplerSpecs* samplers, uint32_t sampler_count, TextureSpecs* textures, uint32_t texture_count, MaterialSpecs* materials, uint32_t material_count);
	SkyboxId skybox_create(const ImageSpecs& texture);
	
	VertexBufferId vertex_buffer_create(const Vertex* data, size_t count);
	IndexBufferId index_buffer_create(const uint32_t* data, size_t count);

private:
	VulkanGraphicsController m_graphics_controller;

	// Defalut shapes
	struct Shape {
		BufferId vertex_buffer;
		BufferId index_buffer;
		uint32_t index_count;
		IndexType index_type;
	} m_square, m_box;

	struct LightInfo {
		alignas(16)	glm::vec3 camera_pos;
		alignas(16) glm::vec3 light_dir;
		alignas(16) glm::vec3 light_color;
		alignas(16) glm::vec3 ambient_color;
	};

	struct SceneInfo {
		struct Data {
			LightInfo light_info;
			Camera camera;
			float exposure = 1.0f;
			float gamma = 2.2f;
		} data;
		
		struct GPU {
			BufferId view_pos; // vec3
			BufferId projview_matrix; // mat4
			BufferId projview_matrix_no_translation; // mat4
		} gpu;
	} m_scene_info;

	struct Deferred {
		ImageInfo albedo_info;
		ImageInfo ao_rough_met_info;
		ImageInfo normals_info;
		ImageInfo emissive_info;
		ImageInfo depth_stencil_info;
		ImageInfo composition_info;

		ImageId albedo;
		ImageId ao_rough_met;
		ImageId normals;
		ImageId emissive;
		ImageId depth_stencil;
		ImageId composition;

		RenderPassId g_pass;
		FramebufferId g_framebuffer;
		RenderPassId depth_copy_pass;
		FramebufferId depth_copy_framebuffer;
		RenderPassId composition_pass;
		FramebufferId composition_framebuffer;
		RenderPassId present_pass;
		FramebufferId present_framebuffer;
	} m_deferred;

	struct GPipeline {
		ShaderId shader;
		PipelineId pipeline;
		UniformSetId uniform_set_0;
	} m_g_pipeline;

	struct LightningPipeline {
		ShaderId shader;
		PipelineId pipeline;
		SamplerId sampler;
		UniformSetId uniform_set_0;
	} m_light_pipeline;

	struct BlendPipeline {
		ShaderId shader;
		PipelineId pipeline;
		BufferId uniform_buffer;
		UniformSetId uniform_set_0;
	} m_blend_pipeline;

	struct SkyboxPipeline {
		ShaderId shader;
		PipelineId pipeline;
		SamplerId sampler;
		UniformSetId uniform_set_0;
	} m_skybox_pipeline;

	struct CoordSystemPipeline {
		ShaderId shader;
		PipelineId pipeline;
		VertexBufferId vertex_buffer;
		UniformSetId uniform_set_0;
	} m_coord_system_pipeline;

	struct PresentationPipeline {
		Shape square;
		ShaderId shader;
		PipelineId pipeline;
		SamplerId same_res_sampler;
		SamplerId diff_res_sampler;
		UniformSetId uniform_set_0;
	} m_present_pipeline;

	struct GenerateCubemapPipeline {
		RenderPassId render_pass;
		ShaderId shader;
		PipelineId pipeline;
		BufferId views;
	} m_gen_cubemap_pipeline;

	// Skybox
	struct Skybox {
		ImageId image;
		UniformSetId uniform_set_1;
	};

	struct Texture {
		SamplerId sampler;
		ImageId image;
	};

	struct Material {
		MaterialInfo info;
		AlphaMode alpha_mode;
		bool has_albedo_map;
		bool has_ao_rough_met_map;
		bool has_normal_map;
		bool has_emissive_map;
		UniformSetId uniform_set;
	};

	struct Primitive {
		glm::mat4 model;
		size_t vertex_buffer;
		size_t index_buffer;
		size_t first_index;
		size_t index_count;
		size_t vertex_count;
		MaterialId material;
	};

	struct Defaults {
		Texture empty_texture;
	} m_defaults;

	std::vector<BufferId> m_vertex_buffers;
	std::vector<BufferId> m_index_buffers;
	std::vector<Material> m_materials;
	std::vector<Skybox> m_skyboxes;
	std::vector<Light> m_lights;

	// Draw list
	struct DrawList {
		Light dir_light;
		std::vector<Light> point_lights;
		std::vector<Primitive> opaque_primitives;
		std::vector<Primitive> blend_primitives;
		std::optional<SkyboxId> skybox;

		void clear() {
			opaque_primitives.clear();
			blend_primitives.clear();
			point_lights.clear();
			skybox.reset();
		}
	};

	DrawList m_draw_list;
};