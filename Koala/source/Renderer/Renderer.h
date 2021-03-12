#pragma once

#include "VulkanContext.h"
#include "VulkanGraphicsController.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <optional>

using SkyboxId = uint32_t;
using MeshId = uint32_t;

struct Vertex {
	glm::vec3 pos;
	glm::vec2 tex_pos;

	bool operator==(const Vertex& v) const {
		return pos == v.pos && tex_pos == v.tex_pos;
	}
};

struct TextureSpecification {
	uint32_t width;
	uint32_t height;
	const void* data;
};

struct Camera {
	glm::vec3 eye = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 front;
	glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);

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

class Renderer {
public:
	void create(VulkanContext* context);
	void destroy();

	void set_resolution(uint32_t width, uint32_t height);
	void set_shadow_map_resolution(uint32_t width, uint32_t height);

	void set_directional_light(const Camera& camera);

	void begin_frame(const Camera& camera);
	void end_frame(uint32_t width, uint32_t height);

	void draw_skybox(SkyboxId skybox_id);
	void draw_mesh(MeshId mesh_id);

	SkyboxId skybox_create(const TextureSpecification& texture);

	MeshId mesh_create(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const TextureSpecification& texture);
	void mesh_update_model_matrix(MeshId mesh_id, const glm::mat4& model);

private:
	VulkanGraphicsController m_graphics_controller;

	// Render Target
	struct RenderTarget {
		std::vector<ImageInfo> image_infos;
		std::vector<ImageId> images;

		RenderPassId render_pass;
		FramebufferId framebuffer;
	};

	RenderTarget m_offscreen_render_target;
	RenderTarget m_shadow_map_target;

	struct ScreenPresentation {
		ShaderId shader;
		PipelineId pipeline;
		SamplerId sampler;
		UniformSetId uniform_set_0;
	};

	ScreenPresentation m_screen_presentation;

	struct ShadowGeneration {
		ShaderId shader;
		PipelineId pipeline;
		BufferId light_world_matrix;
		UniformSetId shadow_gen_uniform_set_0;
	};

	ShadowGeneration m_shadow_generation;

	// Defalut shapes
	struct Shape {
		BufferId vertex_buffer;
		BufferId index_buffer;
		uint32_t index_count;
		IndexType index_type;
	};

	Shape m_square;
	Shape m_box;

	// World-Space Camera
	BufferId m_world_space_proj_view_matrix;

	// Skybox
	struct Skybox {
		ImageId texture;
		SamplerId sampler;
		UniformSetId uniform_set_0;
		UniformSetId uniform_set_1;
	};

	ShaderId m_skybox_shader;
	PipelineId m_skybox_pipeline;
	BufferId m_skybox_proj_view_matrix;
	std::vector<Skybox> m_skyboxes;

	// Mesh
	struct Mesh {
		BufferId vertex_buffer;
		BufferId index_buffer;
		uint32_t index_count;

		BufferId model_uniform_buffer;
		ImageId texture;

		UniformSetId draw_uniform_set_0;
		UniformSetId shadow_gen_uniform_set_1;
	};

	struct MeshCommon {
		ShaderId shader;
		PipelineId pipeline;
		SamplerId texture_sampler;
		SamplerId shadow_map_sampler;
		UniformSetId draw_uniform_set_1;
	};

	MeshCommon m_mesh_common;
	std::vector<Mesh> m_meshes;

	void create_mesh_uniform_set_1();

	// Draw list
	struct DrawList {
		std::vector<MeshId> meshes;
		std::optional<SkyboxId> skybox;

		void clear() {
			meshes.clear();
			skybox.reset();
		}
	};

	DrawList m_draw_list;
};