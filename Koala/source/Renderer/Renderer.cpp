#include "Renderer.h"

#include <filesystem>
#include <fstream>

#include <stb_image/stb_image.h>

static std::vector<uint8_t> load_spv(const std::filesystem::path& path) {
	if (!std::filesystem::exists(path))
		throw std::runtime_error("Shader doesn't exist");

	size_t code_size = std::filesystem::file_size(path);
	size_t zeros_count = (4 - (code_size % 4)) % 4;

	std::vector<uint8_t> spv_code;
	spv_code.reserve(code_size + zeros_count);

	std::basic_ifstream<uint8_t> spv_file(path, std::ios::binary);

	spv_code.insert(spv_code.end(), std::istreambuf_iterator<uint8_t>(spv_file), std::istreambuf_iterator<uint8_t>());
	spv_code.insert(spv_code.end(), zeros_count, 0);

	return spv_code;
}

void Renderer::create(VulkanContext* context) {
	m_graphics_controller.create(context);

	// Setup presentation
	m_render_target.color_attachment_image_info = {
		.usage = ImageUsageColorAttachment | ImageUsageSampled,
		.view_type = ImageViewType::TwoD,
		.format = Format::RGBA32_SFloat,
		.depth = 1,
		.layer_count = 1
	};

	m_render_target.depth_attachment_image_info = {
		.usage = ImageUsageDepthStencilAttachment,
		.view_type = ImageViewType::TwoD,
		.format = Format::D32_SFloat,
		.depth = 1,
		.layer_count = 1
	};

	{
		// Create offscreen render pass
		std::array<RenderPassAttachment, 2> attachments{};
		attachments[0].format = m_render_target.color_attachment_image_info.format;
		attachments[0].usage = m_render_target.color_attachment_image_info.usage;
		attachments[0].initial_action = InitialAction::Clear;
		attachments[0].final_action = FinalAction::Store;
		attachments[1].format = m_render_target.depth_attachment_image_info.format;
		attachments[1].usage = m_render_target.depth_attachment_image_info.usage;
		attachments[1].initial_action = InitialAction::Clear;
		attachments[1].final_action = FinalAction::DontCare;

		m_render_target.render_pass = m_graphics_controller.render_pass_create(attachments.data(), (uint32_t)attachments.size());

		// Create offscreen shader, pipeline and uniforms
		m_render_target.shader = m_graphics_controller.shader_create(load_spv("../assets/shaders/display.vert.spv"), load_spv("../assets/shaders/display.frag.spv"));

		std::array<PipelineDynamicStateFlags, 2> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR };

		PipelineInfo display_pipeline_info{};
		display_pipeline_info.shader_id = m_render_target.shader;
		display_pipeline_info.assembly.topology = PrimitiveTopology::TriangleList;
		display_pipeline_info.assembly.restart_enable = false;
		display_pipeline_info.raster.depth_clamp_enable = false;
		display_pipeline_info.raster.rasterizer_discard_enable = false;
		display_pipeline_info.raster.polygon_mode = PolygonMode::Fill;
		display_pipeline_info.raster.cull_mode = CullMode::None;
		display_pipeline_info.raster.depth_bias_enable = false;
		display_pipeline_info.raster.line_width = 1.0f;
		display_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
		display_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();

		m_render_target.pipeline = m_graphics_controller.pipeline_create(display_pipeline_info);

		SamplerInfo sampler_info{};

		m_render_target.sampler = m_graphics_controller.sampler_create(sampler_info);
	}

	// Create wordl-space proj-view matrix
	m_world_space_proj_view_matrix = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));

	// Create default shapes
	{ // Square
		float vertices[4 * 4] = {
			-1.0f, -1.0f, 0.0f, 0.0f,
			 1.0f, -1.0f, 1.0f, 0.0f,
			-1.0f,  1.0f, 0.0f, 1.0f,
			 1.0f,  1.0f, 1.0f, 1.0f
		};

		m_square.index_count = 6;
		m_square.index_type = IndexType::Uint32;
		uint32_t indices[6] = {
			0, 1, 2,
			1, 2, 3
		};

		m_square.vertex_buffer = m_graphics_controller.vertex_buffer_create(vertices, sizeof(vertices));
		m_square.index_buffer = m_graphics_controller.index_buffer_create(indices, m_square.index_count * sizeof(uint32_t), m_square.index_type);
	}
	
	{ // 1 * 1 * 1 box
		float vertices[36 * 3] = {
			-1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			-1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f
		};

		m_box.index_count = 36;
		m_box.index_type = IndexType::Uint32;
		uint32_t indices[] = {
			 0,  1,  2,  3,  4,  5,
			 6,  7,  8,  9, 10, 11,
			12, 13, 14, 15, 16, 17,
			18, 19, 20, 21, 22, 23,
			24, 25, 26, 27, 28, 29,
			30, 31, 32, 33, 34, 35
		};

		m_box.vertex_buffer = m_graphics_controller.vertex_buffer_create(vertices, sizeof(vertices));
		m_box.index_buffer = m_graphics_controller.index_buffer_create(indices, sizeof(indices), m_box.index_type);
	}

	// Create defalut shaders and pipelines
	{ // Skybox pipeline
		m_skybox_shader = m_graphics_controller.shader_create(
			load_spv("../assets/shaders/skybox.vert.spv"),
			load_spv("../assets/shaders/skybox.frag.spv")
		);

		std::array<PipelineDynamicStateFlags, 2> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR };

		PipelineInfo skybox_pipeline_info{};
		skybox_pipeline_info.shader_id = m_skybox_shader;
		skybox_pipeline_info.assembly.topology = PrimitiveTopology::TriangleList;
		skybox_pipeline_info.assembly.restart_enable = false;
		skybox_pipeline_info.raster.depth_clamp_enable = false;
		skybox_pipeline_info.raster.rasterizer_discard_enable = false;
		skybox_pipeline_info.raster.polygon_mode = PolygonMode::Fill;
		skybox_pipeline_info.raster.cull_mode = CullMode::None;
		skybox_pipeline_info.raster.depth_bias_enable = false;
		skybox_pipeline_info.raster.line_width = 1.0f;
		skybox_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
		skybox_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();
		skybox_pipeline_info.render_pass_id = m_render_target.render_pass;

		m_skybox_pipeline = m_graphics_controller.pipeline_create(skybox_pipeline_info);

		m_skybox_proj_view_matrix = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));
	}

	{ // Default mesh pipeline
		m_mesh_shader = m_graphics_controller.shader_create(
			load_spv("../assets/shaders/vertex.spv"),
			load_spv("../assets/shaders/fragment.spv")
		);

		std::array<PipelineDynamicStateFlags, 2> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR };

		PipelineInfo mesh_pipeline_info{};
		mesh_pipeline_info.shader_id = m_mesh_shader;
		mesh_pipeline_info.assembly.topology = PrimitiveTopology::TriangleList;
		mesh_pipeline_info.assembly.restart_enable = false;
		mesh_pipeline_info.raster.depth_clamp_enable = false;
		mesh_pipeline_info.raster.rasterizer_discard_enable = false;
		mesh_pipeline_info.raster.polygon_mode = PolygonMode::Fill;
		mesh_pipeline_info.raster.cull_mode = CullMode::None;
		mesh_pipeline_info.raster.depth_bias_enable = false;
		mesh_pipeline_info.raster.line_width = 1.0f;
		mesh_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
		mesh_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();
		mesh_pipeline_info.render_pass_id = m_render_target.render_pass;

		m_mesh_pipeline = m_graphics_controller.pipeline_create(mesh_pipeline_info);
	}
}

void Renderer::destroy() {
	m_graphics_controller.destroy();
}

void Renderer::set_resolution(uint32_t width, uint32_t height) {
	// Create offscreen render target
	m_render_target.color_attachment_image_info.width = width;
	m_render_target.color_attachment_image_info.height = height;
	
	m_render_target.color_attachment = m_graphics_controller.image_create(nullptr, m_render_target.color_attachment_image_info);

	m_render_target.depth_attachment_image_info.width = width;
	m_render_target.depth_attachment_image_info.height = height;

	m_render_target.depth_attachment = m_graphics_controller.image_create(nullptr, m_render_target.depth_attachment_image_info);
	
	ImageId ids[2] = { m_render_target.color_attachment, m_render_target.depth_attachment };

	m_render_target.framebuffer = m_graphics_controller.framebuffer_create(m_render_target.render_pass, ids, 2);

	RenderId binding_0_set_0_ids[2] = { m_render_target.color_attachment, m_render_target.sampler };

	std::array<Uniform, 1> uniform_set0;
	uniform_set0[0].type = UniformType::CombinedImageSampler;
	uniform_set0[0].binding = 0;
	uniform_set0[0].ids = binding_0_set_0_ids;
	uniform_set0[0].id_count = 2;

	m_render_target.uniform_set = m_graphics_controller.uniform_set_create(m_render_target.shader, 0, uniform_set0.data(), (uint32_t)uniform_set0.size());
}

void Renderer::begin_frame(const Camera& camera) {
	glm::mat4 view = camera.view_matrix();
	glm::mat4 proj = camera.proj_matrix();
	glm::mat4 proj_view = proj * view;

	m_graphics_controller.buffer_update(m_world_space_proj_view_matrix, &proj_view);
	
	glm::mat4 view_no_translation = glm::mat4(glm::mat3(view));
	glm::mat4 skybox_view_proj = proj * view_no_translation;
	m_graphics_controller.buffer_update(m_skybox_proj_view_matrix, &skybox_view_proj);

	glm::vec4 clear_values[2] = { { 0.9f, 0.7f, 0.8f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f } };
	m_graphics_controller.draw_begin(m_render_target.framebuffer, clear_values, 2);

	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)m_render_target.color_attachment_image_info.width, (float)m_render_target.color_attachment_image_info.height, 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, m_render_target.color_attachment_image_info.width, m_render_target.color_attachment_image_info.height);
}

void Renderer::end_frame(uint32_t width, uint32_t height) {
	m_graphics_controller.draw_end();

	glm::vec4 clear_color{ 0.0f, 1.0f, 0.0f, 1.0f };
	m_graphics_controller.draw_begin_for_screen(clear_color);

	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, width, height);

	UniformSetId display_sets[1] = { m_render_target.uniform_set };
	m_graphics_controller.draw_bind_pipeline(m_render_target.pipeline);
	m_graphics_controller.draw_bind_vertex_buffer(m_square.vertex_buffer);
	m_graphics_controller.draw_bind_index_buffer(m_square.index_buffer, m_square.index_type);
	m_graphics_controller.draw_bind_uniform_sets(m_render_target.pipeline, display_sets, 1);
	m_graphics_controller.draw_draw_indexed(m_square.index_count);

	m_graphics_controller.draw_end_for_screen();

	m_graphics_controller.end_frame();
}

void Renderer::draw_skybox(SkyboxId skybox_id) {
	const Skybox& skybox = m_skyboxes[skybox_id];

	UniformSetId skybox_sets[2] = { skybox.uniform_set_0, skybox.uniform_set_1 };
	m_graphics_controller.draw_bind_pipeline(m_skybox_pipeline);
	m_graphics_controller.draw_bind_vertex_buffer(m_box.vertex_buffer);
	m_graphics_controller.draw_bind_index_buffer(m_box.index_buffer, m_box.index_type);
	m_graphics_controller.draw_bind_uniform_sets(m_skybox_pipeline, skybox_sets, 2);
	m_graphics_controller.draw_draw_indexed(m_box.index_count);
}

void Renderer::draw_mesh(MeshId mesh_id) {
	const Mesh& mesh = m_meshes[mesh_id];

	UniformSetId model_sets[2] = { mesh.uniform_set_0, mesh.uniform_set_1 };
	m_graphics_controller.draw_bind_pipeline(m_mesh_pipeline);
	m_graphics_controller.draw_bind_vertex_buffer(mesh.vertex_buffer);
	m_graphics_controller.draw_bind_index_buffer(mesh.index_buffer, IndexType::Uint32);
	m_graphics_controller.draw_bind_uniform_sets(m_mesh_pipeline, model_sets, 2);
	m_graphics_controller.draw_draw_indexed(mesh.index_count);
}

SkyboxId Renderer::skybox_create(const TextureSpecification& texture) {
	Skybox skybox;

	ImageInfo skybox_texture_info = {
		.usage = ImageUsageSampled | ImageUsageTransferDst,
		.view_type = ImageViewType::Cube,
		.format = Format::RGBA8_SRGB,
		.width = (uint32_t)texture.height,
		.height = (uint32_t)texture.width / 6,
		.depth = 1,
		.layer_count = 6
	};

	skybox.texture = m_graphics_controller.image_create(texture.data, skybox_texture_info);

	SamplerInfo sampler_info{};

	skybox.sampler = m_graphics_controller.sampler_create(sampler_info);

	RenderId uniform_0_set_0_ids[1] = { m_skybox_proj_view_matrix };

	std::array<Uniform, 1> skybox_uniform_set0;
	skybox_uniform_set0[0].type = UniformType::UniformBuffer;
	skybox_uniform_set0[0].binding = 0;
	skybox_uniform_set0[0].ids = uniform_0_set_0_ids;
	skybox_uniform_set0[0].id_count = 1;

	RenderId uniform_0_set_1_ids[2] = { skybox.texture, skybox.sampler };

	std::array<Uniform, 1> skybox_uniform_set1;
	skybox_uniform_set1[0].type = UniformType::CombinedImageSampler;
	skybox_uniform_set1[0].binding = 0;
	skybox_uniform_set1[0].ids = uniform_0_set_1_ids;
	skybox_uniform_set1[0].id_count = 2;

	skybox.uniform_set_0 = m_graphics_controller.uniform_set_create(m_skybox_shader, 0, skybox_uniform_set0.data(), (uint32_t)skybox_uniform_set0.size());
	skybox.uniform_set_1 = m_graphics_controller.uniform_set_create(m_skybox_shader, 1, skybox_uniform_set1.data(), (uint32_t)skybox_uniform_set1.size());

	m_skyboxes.push_back(skybox);
	return (RenderId)m_skyboxes.size() - 1;
}

MeshId Renderer::mesh_create(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const TextureSpecification& texture) {
	Mesh mesh;
	
	mesh.vertex_buffer = m_graphics_controller.vertex_buffer_create(vertices.data(), vertices.size() * sizeof(Vertex));
	mesh.index_count = (uint32_t)indices.size();
	mesh.index_buffer = m_graphics_controller.index_buffer_create(indices.data(), indices.size() * sizeof(uint32_t), IndexType::Uint32);

	ImageInfo texture_image_info = {
		.usage = ImageUsageSampled | ImageUsageTransferDst,
		.view_type = ImageViewType::TwoD,
		.format = Format::RGBA8_SRGB,
		.width = (uint32_t)texture.width,
		.height = (uint32_t)texture.height,
		.depth = 1,
		.layer_count = 1
	};

	mesh.texture = m_graphics_controller.image_create(texture.data, texture_image_info);

	SamplerInfo sampler_info{};

	mesh.sampler = m_graphics_controller.sampler_create(sampler_info);

	mesh.model_uniform_buffer = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));
	
	std::array<Uniform, 2> uniform_set0;
	// Projection-View Matrix
	uniform_set0[0].type = UniformType::UniformBuffer;
	uniform_set0[0].binding = 0;
	uniform_set0[0].ids = &m_world_space_proj_view_matrix;
	uniform_set0[0].id_count = 1;
	// Model Matrix
	uniform_set0[1].type = UniformType::UniformBuffer;
	uniform_set0[1].binding = 1;
	uniform_set0[1].ids = &mesh.model_uniform_buffer;
	uniform_set0[1].id_count = 1;

	RenderId binding_0_set_1_ids[2] = { mesh.texture, mesh.sampler };

	std::array<Uniform, 1> uniform_set1;
	uniform_set1[0].type = UniformType::CombinedImageSampler;
	uniform_set1[0].binding = 0;
	uniform_set1[0].ids = binding_0_set_1_ids;
	uniform_set1[0].id_count = 2;

	mesh.uniform_set_0 = m_graphics_controller.uniform_set_create(m_mesh_shader, 0, uniform_set0.data(), (uint32_t)uniform_set0.size());
	mesh.uniform_set_1 = m_graphics_controller.uniform_set_create(m_mesh_shader, 1, uniform_set1.data(), (uint32_t)uniform_set1.size());

	m_meshes.push_back(mesh);
	return (MeshId)m_meshes.size() - 1;
}

void Renderer::mesh_update_model_matrix(MeshId mesh_id, const glm::mat4& model) {
	m_graphics_controller.buffer_update(m_meshes[mesh_id].model_uniform_buffer, &model);
}