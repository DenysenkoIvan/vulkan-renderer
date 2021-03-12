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

	// Create render targets
	// Create offscreen render target
	{
		m_offscreen_render_target.image_infos.resize(2);
		m_offscreen_render_target.images.resize(2);

		m_offscreen_render_target.image_infos[0] = {
			.usage = ImageUsageColorAttachment | ImageUsageSampled,
			.view_type = ImageViewType::TwoD,
			.format = Format::RGBA32_SFloat,
			.depth = 1,
			.layer_count = 1
		};
	
		m_offscreen_render_target.image_infos[1] = {
			.usage = ImageUsageDepthStencilAttachment,
			.view_type = ImageViewType::TwoD,
			.format = Format::D32_SFloat,
			.depth = 1,
			.layer_count = 1
		};

		// Create offscreen render pass
		std::array<RenderPassAttachment, 2> attachments{};
		attachments[0].usage = m_offscreen_render_target.image_infos[0].usage;
		attachments[0].format = m_offscreen_render_target.image_infos[0].format;
		attachments[0].initial_action = InitialAction::Clear;
		attachments[0].final_action = FinalAction::Store;
		attachments[1].usage = m_offscreen_render_target.image_infos[1].usage;
		attachments[1].format = m_offscreen_render_target.image_infos[1].format;
		attachments[1].initial_action = InitialAction::Clear;
		attachments[1].final_action = FinalAction::DontCare;

		m_offscreen_render_target.render_pass = m_graphics_controller.render_pass_create(attachments.data(), (uint32_t)attachments.size());
	}
	
	// Create shadow map render target
	{
		m_shadow_map_target.image_infos.resize(1);
		m_shadow_map_target.images.resize(1);

		m_shadow_map_target.image_infos[0] = {
			.usage = ImageUsageDepthStencilAttachment | ImageUsageSampled,
			.view_type = ImageViewType::TwoD,
			.format = Format::D32_SFloat,
			.depth = 1,
			.layer_count = 1
		};
	
		RenderPassAttachment attachment{
			.usage = m_shadow_map_target.image_infos[0].usage,
			.format = m_shadow_map_target.image_infos[0].format,
			.initial_action = InitialAction::Clear,
			.final_action = FinalAction::Store
		};
		
		m_shadow_map_target.render_pass = m_graphics_controller.render_pass_create(&attachment, 1);
	}

	// Create shadow map shader and pipeline
	{
		std::vector<uint8_t> vertex_shader = load_spv("../assets/shaders/shadow_map.vert.spv");

		m_shadow_generation.shader = m_graphics_controller.shader_create(vertex_shader.data(), (uint32_t)vertex_shader.size(), nullptr, 0);
	
		std::array<PipelineDynamicStateFlags, 2> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR };

		PipelineInfo shadow_map_pipeline_info{};
		shadow_map_pipeline_info.shader_id = m_screen_presentation.shader;
		shadow_map_pipeline_info.assembly.topology = PrimitiveTopology::TriangleList;
		shadow_map_pipeline_info.assembly.restart_enable = false;
		shadow_map_pipeline_info.raster.depth_clamp_enable = false;
		shadow_map_pipeline_info.raster.rasterizer_discard_enable = false;
		shadow_map_pipeline_info.raster.polygon_mode = PolygonMode::Fill;
		shadow_map_pipeline_info.raster.cull_mode = CullMode::None;
		shadow_map_pipeline_info.raster.depth_bias_enable = false;
		shadow_map_pipeline_info.raster.line_width = 1.0f;
		shadow_map_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
		shadow_map_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();
		shadow_map_pipeline_info.render_pass_id = m_shadow_map_target.render_pass;

		m_shadow_generation.pipeline = m_graphics_controller.pipeline_create(shadow_map_pipeline_info);

		m_shadow_generation.light_world_matrix = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));
		
		std::array<Uniform, 1> shadow_gen_uniform_set_0;
		shadow_gen_uniform_set_0[0].type = UniformType::UniformBuffer;
		shadow_gen_uniform_set_0[0].binding = 0;
		shadow_gen_uniform_set_0[0].ids = &m_shadow_generation.light_world_matrix;
		shadow_gen_uniform_set_0[0].id_count = 1;
		
		m_shadow_generation.shadow_gen_uniform_set_0 = m_graphics_controller.uniform_set_create(
			m_shadow_generation.shader,
			0,
			shadow_gen_uniform_set_0.data(),
			(uint32_t)shadow_gen_uniform_set_0.size()
		);
	}

	// Create presentation shader, pipeline and uniform
	{
		auto vert_spv = load_spv("../assets/shaders/display.vert.spv");
		auto frag_spv = load_spv("../assets/shaders/display.frag.spv");
		m_screen_presentation.shader = m_graphics_controller.shader_create(vert_spv.data(), (uint32_t)vert_spv.size(), frag_spv.data(), (uint32_t)frag_spv.size());

		std::array<PipelineDynamicStateFlags, 2> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR };

		PipelineInfo display_pipeline_info{};
		display_pipeline_info.shader_id = m_screen_presentation.shader;
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

		m_screen_presentation.pipeline = m_graphics_controller.pipeline_create(display_pipeline_info);

		SamplerInfo sampler_info{};

		m_screen_presentation.sampler = m_graphics_controller.sampler_create(sampler_info);
	}

	// Create world-space proj-view matrix
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
	// Skybox pipeline
	{
		auto vert_spv = load_spv("../assets/shaders/skybox.vert.spv");
		auto frag_spv = load_spv("../assets/shaders/skybox.frag.spv");
		m_skybox_shader = m_graphics_controller.shader_create(vert_spv.data(), (uint32_t)vert_spv.size(), frag_spv.data(), (uint32_t)frag_spv.size());

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
		skybox_pipeline_info.render_pass_id = m_offscreen_render_target.render_pass;

		m_skybox_pipeline = m_graphics_controller.pipeline_create(skybox_pipeline_info);

		m_skybox_proj_view_matrix = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));
	}

	// Mesh pipeline
	{
		auto vert_spv = load_spv("../assets/shaders/vertex.spv");
		auto frag_spv = load_spv("../assets/shaders/fragment.spv");
		m_mesh_common.shader = m_graphics_controller.shader_create(vert_spv.data(), (uint32_t)vert_spv.size(), frag_spv.data(), (uint32_t)frag_spv.size());
		
		std::array<PipelineDynamicStateFlags, 2> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR };

		PipelineInfo mesh_pipeline_info{};
		mesh_pipeline_info.shader_id = m_mesh_common.shader;
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
		mesh_pipeline_info.render_pass_id = m_offscreen_render_target.render_pass;

		m_mesh_common.pipeline = m_graphics_controller.pipeline_create(mesh_pipeline_info);

		SamplerInfo texture_sampler{};
		m_mesh_common.texture_sampler = m_graphics_controller.sampler_create(texture_sampler);

		SamplerInfo sampler_info{};
		sampler_info.min_filter = Filter::Nearest;
		sampler_info.mag_filter = Filter::Nearest;

		m_mesh_common.shadow_map_sampler = m_graphics_controller.sampler_create(sampler_info);
	}

	set_shadow_map_resolution(2048, 2048);
}

void Renderer::destroy() {
	m_graphics_controller.destroy();
}

void Renderer::set_resolution(uint32_t width, uint32_t height) {
	// Create offscreen render target
	m_offscreen_render_target.image_infos[0].width = width;
	m_offscreen_render_target.image_infos[0].height = height;
	
	m_offscreen_render_target.images[0] = m_graphics_controller.image_create(nullptr, m_offscreen_render_target.image_infos[0]);

	m_offscreen_render_target.image_infos[1].width = width;
	m_offscreen_render_target.image_infos[1].height = height;

	m_offscreen_render_target.images[1] = m_graphics_controller.image_create(nullptr, m_offscreen_render_target.image_infos[1]);
	
	ImageId image_ids[2] = { m_offscreen_render_target.images[0], m_offscreen_render_target.images[1] };
	m_offscreen_render_target.framebuffer = m_graphics_controller.framebuffer_create(m_offscreen_render_target.render_pass, image_ids, 2);

	RenderId binding_0_set_0_ids[2] = { m_offscreen_render_target.images[0], m_screen_presentation.sampler };

	std::array<Uniform, 1> uniform_set0;
	uniform_set0[0].type = UniformType::CombinedImageSampler;
	uniform_set0[0].binding = 0;
	uniform_set0[0].ids = binding_0_set_0_ids;
	uniform_set0[0].id_count = 2;

	m_screen_presentation.uniform_set_0 = m_graphics_controller.uniform_set_create(m_screen_presentation.shader, 0, uniform_set0.data(), (uint32_t)uniform_set0.size());
}

void Renderer::set_shadow_map_resolution(uint32_t width, uint32_t height) {
	m_shadow_map_target.image_infos[0].width = width;
	m_shadow_map_target.image_infos[0].height = height;
	
	m_shadow_map_target.images[0] = m_graphics_controller.image_create(nullptr, m_shadow_map_target.image_infos[0]);

	ImageId image_ids[1] = { m_shadow_map_target.images[0] };

	m_shadow_map_target.framebuffer = m_graphics_controller.framebuffer_create(m_shadow_map_target.render_pass, image_ids, 1);

	create_mesh_uniform_set_1();
}

void Renderer::set_directional_light(const Camera& camera) {
	glm::mat4 view = camera.view_matrix();
	float x = 3.0f;
	float y = 3.0f;

	//view = glm::lookAt(glm::vec3(-2.0f, 4.0f, -1.0f),
	//				   glm::vec3( 0.0f, 0.0f,  0.0f),
	//				   glm::vec3( 0.0f, 0.0f,  1.0f)
	//);
	glm::mat4 proj = glm::ortho(-x, x, -y, y, 0.1f, 10.0f);

	glm::mat4 proj_view = proj * view;

	m_graphics_controller.buffer_update(m_shadow_generation.light_world_matrix, &proj_view);
}

void Renderer::begin_frame(const Camera& camera) {
	glm::mat4 view = camera.view_matrix();
	glm::mat4 proj = camera.proj_matrix();
	glm::mat4 proj_view = proj * view;

	m_graphics_controller.buffer_update(m_world_space_proj_view_matrix, &proj_view);
	
	glm::mat4 view_no_translation = glm::mat4(glm::mat3(view));
	glm::mat4 skybox_view_proj = proj * view_no_translation;
	m_graphics_controller.buffer_update(m_skybox_proj_view_matrix, &skybox_view_proj);

	m_draw_list.clear();
}

void Renderer::end_frame(uint32_t width, uint32_t height) {
	// Generate Shadow map
	glm::vec4 shadow_map_clear_values{ 1.0f, 1.0f, 1.0f, 1.0f };
	m_graphics_controller.draw_begin(m_shadow_map_target.framebuffer, &shadow_map_clear_values, 1);

	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)m_shadow_map_target.image_infos[0].width, (float)m_shadow_map_target.image_infos[0].height, 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, m_shadow_map_target.image_infos[0].width, m_shadow_map_target.image_infos[0].height);

	m_graphics_controller.draw_bind_pipeline(m_shadow_generation.pipeline);
	m_graphics_controller.draw_bind_uniform_sets(m_shadow_generation.pipeline, 0, &m_shadow_generation.shadow_gen_uniform_set_0, 1);
	for (MeshId mesh_id : m_draw_list.meshes) {
		const Mesh& mesh = m_meshes[mesh_id];

		m_graphics_controller.draw_bind_vertex_buffer(mesh.vertex_buffer);
		m_graphics_controller.draw_bind_index_buffer(mesh.index_buffer, IndexType::Uint32);
		m_graphics_controller.draw_bind_uniform_sets(m_shadow_generation.pipeline, 1, &mesh.shadow_gen_uniform_set_1, 1);
		m_graphics_controller.draw_draw_indexed(mesh.index_count);
	}

	m_graphics_controller.draw_end();

	// Draw to offscreen buffer
	glm::vec4 offscreen_clear_values[2] = { { 0.9f, 0.7f, 0.8f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f } };
	m_graphics_controller.draw_begin(m_offscreen_render_target.framebuffer, offscreen_clear_values, 2);

	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)m_offscreen_render_target.image_infos[0].width, (float)m_offscreen_render_target.image_infos[0].height, 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, m_offscreen_render_target.image_infos[0].width, m_offscreen_render_target.image_infos[0].height);

	// Draw meshes
	m_graphics_controller.draw_bind_pipeline(m_mesh_common.pipeline);
	m_graphics_controller.draw_bind_uniform_sets(m_mesh_common.pipeline, 1, &m_mesh_common.draw_uniform_set_1, 1);
	for (MeshId mesh_id : m_draw_list.meshes) {
		const Mesh& mesh = m_meshes[mesh_id];
		
		m_graphics_controller.draw_bind_vertex_buffer(mesh.vertex_buffer);
		m_graphics_controller.draw_bind_index_buffer(mesh.index_buffer, IndexType::Uint32);
		m_graphics_controller.draw_bind_uniform_sets(m_mesh_common.pipeline, 0, &mesh.draw_uniform_set_0, 1);
		m_graphics_controller.draw_draw_indexed(mesh.index_count);
	}

	// Draw skybox
	if (m_draw_list.skybox.has_value()) {
		const Skybox& skybox = m_skyboxes[m_draw_list.skybox.value()];

		UniformSetId skybox_sets[2] = { skybox.uniform_set_0, skybox.uniform_set_1 };
		m_graphics_controller.draw_bind_pipeline(m_skybox_pipeline);
		m_graphics_controller.draw_bind_vertex_buffer(m_box.vertex_buffer);
		m_graphics_controller.draw_bind_index_buffer(m_box.index_buffer, m_box.index_type);
		m_graphics_controller.draw_bind_uniform_sets(m_skybox_pipeline, 0, skybox_sets, 2);
		m_graphics_controller.draw_draw_indexed(m_box.index_count);
	}

	m_graphics_controller.draw_end();
	
	// Present to screen
	glm::vec4 clear_color{ 0.0f, 1.0f, 0.0f, 1.0f };
	m_graphics_controller.draw_begin_for_screen(clear_color);

	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, width, height);

	m_graphics_controller.draw_bind_pipeline(m_screen_presentation.pipeline);
	m_graphics_controller.draw_bind_vertex_buffer(m_square.vertex_buffer);
	m_graphics_controller.draw_bind_index_buffer(m_square.index_buffer, m_square.index_type);
	m_graphics_controller.draw_bind_uniform_sets(m_screen_presentation.pipeline, 0, &m_screen_presentation.uniform_set_0, 1);
	m_graphics_controller.draw_draw_indexed(m_square.index_count);

	m_graphics_controller.draw_end_for_screen();

	m_graphics_controller.end_frame();
}

void Renderer::draw_skybox(SkyboxId skybox_id) {
	m_draw_list.skybox = skybox_id;
}

void Renderer::draw_mesh(MeshId mesh_id) {
	m_draw_list.meshes.push_back(mesh_id);
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

	mesh.model_uniform_buffer = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));
	
	// Per mesh uniform set
	RenderId texture_sampler_ids[2] = { mesh.texture, m_mesh_common.texture_sampler };

	std::array<Uniform, 2> model_texture_uniform_set_0;
	model_texture_uniform_set_0[0].type = UniformType::UniformBuffer;
	model_texture_uniform_set_0[0].binding = 0;
	model_texture_uniform_set_0[0].ids = &mesh.model_uniform_buffer;
	model_texture_uniform_set_0[0].id_count = 1;
	model_texture_uniform_set_0[1].type = UniformType::CombinedImageSampler;
	model_texture_uniform_set_0[1].binding = 1;
	model_texture_uniform_set_0[1].ids = texture_sampler_ids;
	model_texture_uniform_set_0[1].id_count = 2;

	mesh.draw_uniform_set_0 = m_graphics_controller.uniform_set_create(
		m_mesh_common.shader,
		0,
		model_texture_uniform_set_0.data(),
		(uint32_t)model_texture_uniform_set_0.size()
	);

	// Shadow generation uniform set
	mesh.shadow_gen_uniform_set_1 = m_graphics_controller.uniform_set_create(
		m_shadow_generation.shader,
		1,
		model_texture_uniform_set_0.data(),
		1
	);

	m_meshes.push_back(mesh);
	return (MeshId)m_meshes.size() - 1;
}

void Renderer::mesh_update_model_matrix(MeshId mesh_id, const glm::mat4& model) {
	m_graphics_controller.buffer_update(m_meshes[mesh_id].model_uniform_buffer, &model);
}

void Renderer::create_mesh_uniform_set_1() {
	
	RenderId shadow_map_texture_sampler_ids[2] = { m_shadow_map_target.images[0], m_mesh_common.shadow_map_sampler };

	std::array<Uniform, 3> shadow_map_uniform_set_1;
	shadow_map_uniform_set_1[0].type = UniformType::UniformBuffer;
	shadow_map_uniform_set_1[0].binding = 0;
	shadow_map_uniform_set_1[0].ids = &m_world_space_proj_view_matrix;
	shadow_map_uniform_set_1[0].id_count = 1;
	shadow_map_uniform_set_1[1].type = UniformType::UniformBuffer;
	shadow_map_uniform_set_1[1].binding = 1;
	shadow_map_uniform_set_1[1].ids = &m_shadow_generation.light_world_matrix;
	shadow_map_uniform_set_1[1].id_count = 1;
	shadow_map_uniform_set_1[2].type = UniformType::CombinedImageSampler;
	shadow_map_uniform_set_1[2].binding = 2;
	shadow_map_uniform_set_1[2].ids = shadow_map_texture_sampler_ids;
	shadow_map_uniform_set_1[2].id_count = 2;

	m_mesh_common.draw_uniform_set_1 = m_graphics_controller.uniform_set_create(
		m_mesh_common.shader,
		1,
		shadow_map_uniform_set_1.data(),
		(uint32_t)shadow_map_uniform_set_1.size()
	);
}