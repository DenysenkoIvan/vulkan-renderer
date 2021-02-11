#include "Application.h"

#include <Renderer/VulkanContext.h>
#include <stb_image/stb_image.h>
#include <tiny_obj_loader/tiny_obj_loader.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

Application::Application(const ApplicationProperties& props) {
	WindowProperties window_props;
	window_props.width = props.width;
	window_props.height = props.height;
	window_props.title = props.app_name;
	window_props.callback = ([this](Event& e) { this->on_event(e); });

	m_window.initialize(window_props);
	m_graphics_controller.create(m_window.context());

	m_color_attachment = m_graphics_controller.image_create(nullptr, ImageUsageColorAttachment, Format::RGBA32_SFloat, m_window.width(), m_window.height());
	m_depth_attachment = m_graphics_controller.image_create(nullptr, ImageUsageDepthStencilAttachment | ImageUsageSampled, Format::D32_SFloat, m_window.width(), m_window.height());

	std::array<RenderPassAttachment, 2> attachments{};
	attachments[0].format = Format::RGBA32_SFloat;
	attachments[0].usage = ImageUsageColorAttachment;
	attachments[0].initial_action = InitialAction::Clear;
	attachments[0].final_action = FinalAction::Store;
	attachments[1].format = Format::D32_SFloat;
	attachments[1].usage = ImageUsageDepthStencilAttachment;
	attachments[1].initial_action = InitialAction::Clear;
	attachments[1].final_action = FinalAction::DontCare;

	m_render_pass = m_graphics_controller.render_pass_create(attachments.data(), (uint32_t)attachments.size());
	
	ImageId ids[2] = { m_color_attachment, m_depth_attachment };

	m_framebuffer = m_graphics_controller.framebuffer_create(m_render_pass, ids, 2);

	m_mvp.model = glm::mat4(1);
	m_mvp.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	m_mvp.proj = glm::perspective(glm::radians(45.0f), m_window.width() / (float)m_window.height(), 0.1f, 10.0f);
	m_mvp.proj[1][1] *= -1;

	auto load_spv = [](const std::filesystem::path& path) -> std::vector<uint8_t> {
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
	};

	m_hdr_shader = m_graphics_controller.shader_create(load_spv("../assets/shaders/vertex.spv"), load_spv("../assets/shaders/fragment.spv"));

	m_display_shader = m_graphics_controller.shader_create(load_spv("../assets/shaders/display.vert.spv"), load_spv("../assets/shaders/display.frag.spv"));

	PipelineInfo hdr_pipeline_info{};
	hdr_pipeline_info.shader_id = m_hdr_shader;
	hdr_pipeline_info.assembly.topology = PrimitiveTopology::TriangleList;
	hdr_pipeline_info.assembly.restart_enable = false;
	hdr_pipeline_info.raster.depth_clamp_enable = false;
	hdr_pipeline_info.raster.rasterizer_discard_enable = false;
	hdr_pipeline_info.raster.polygon_mode = PolygonMode::Fill;
	hdr_pipeline_info.raster.cull_mode = CullMode::None;
	hdr_pipeline_info.raster.depth_bias_enable = false;
	hdr_pipeline_info.raster.line_width = 1.0f;
	hdr_pipeline_info.render_pass_id = m_render_pass;

	m_hdr_pipeline = m_graphics_controller.pipeline_create(&hdr_pipeline_info);

	PipelineInfo display_pipeline_info{};
	display_pipeline_info.shader_id = m_display_shader;
	display_pipeline_info.assembly.topology = PrimitiveTopology::TriangleList;
	display_pipeline_info.assembly.restart_enable = false;
	display_pipeline_info.raster.depth_clamp_enable = false;
	display_pipeline_info.raster.rasterizer_discard_enable = false;
	display_pipeline_info.raster.polygon_mode = PolygonMode::Fill;
	display_pipeline_info.raster.cull_mode = CullMode::None;
	display_pipeline_info.raster.depth_bias_enable = false;
	display_pipeline_info.raster.line_width = 1.0f;

	m_display_pipeline = m_graphics_controller.pipeline_create(&display_pipeline_info);

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "../assets/models/viking_room.obj"))
		throw std::runtime_error(warn + err);

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			Vertex vertex{};

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.tex_pos = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
				m_vertices.push_back(vertex);
			}

			m_indices.push_back(uniqueVertices[vertex]);
		}
	}

	m_vertex_buffer = m_graphics_controller.vertex_buffer_create(m_vertices.data(), m_vertices.size() * sizeof(Vertex));
	m_index_type = IndexType::Uint32;
	m_index_count = (uint32_t)m_indices.size();
	m_index_buffer = m_graphics_controller.index_buffer_create(m_indices.data(), m_indices.size() * sizeof(uint32_t), m_index_type);

	int width = 0, height = 0, channels = 0;
	stbi_uc* pixels = stbi_load("../assets/models/viking_room.png", &width, &height, &channels, STBI_rgb_alpha);
	
	if (!pixels)
		throw std::runtime_error("Failed to load image");
	
	m_uniform_buffer = m_graphics_controller.uniform_buffer_create(&m_mvp, sizeof(MVP));
	
	m_texture = m_graphics_controller.image_create(pixels, ImageUsageSampled | ImageUsageTransferDst, Format::RGBA8_SRGB, width, height);
	
	SamplerInfo sampler_info{};

	m_sampler = m_graphics_controller.sampler_create(sampler_info);

	std::vector<Uniform> uniform_set0;
	uniform_set0.reserve(1);
	std::vector<Uniform> uniform_set1;
	uniform_set1.reserve(2);
	
	Uniform uniform_buffer;
	uniform_buffer.type = UniformType::UniformBuffer;
	uniform_buffer.binding = 0;
	uniform_buffer.ids.push_back(m_uniform_buffer);

	Uniform texture;
	texture.type = UniformType::CombinedImageSampler;
	texture.binding = 0;
	texture.ids.push_back(m_texture);
	texture.ids.push_back(m_sampler);

	uniform_set0.push_back(std::move(uniform_buffer));

	uniform_set1.push_back(std::move(texture));

	m_uniform_set0 = m_graphics_controller.uniform_set_create(m_hdr_shader, 0, uniform_set0);
	m_uniform_set1 = m_graphics_controller.uniform_set_create(m_hdr_shader, 1, uniform_set1);
}

Application::~Application() {
	m_graphics_controller.destroy();
	//m_renderer.destroy();
}

void Application::on_event(Event& e) {
	EventDispatcher dispatcher(e);

	dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) { this->on_window_resize(e); });
	dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) { this->on_window_close(e); });
	//if (dispatcher.is_dispatched())
	//	return;

	for (auto it = m_layer_stack.rbegin(); it != m_layer_stack.rend(); it++) {
		if ((*it)->on_event(e))
			break;
	}
}

void Application::run() {
	while (m_running) {
		m_window.on_update();

		on_update();

		if (!m_window.is_minimized())
			on_render();
	}
}

void Application::on_window_close(WindowCloseEvent& e) {
	m_running = false;
}

void Application::on_window_resize(WindowResizeEvent& e) {
	m_graphics_controller.resize(e.width(), e.height());
	m_mvp.proj = glm::perspective(glm::radians(45.0f), m_window.width() / (float)m_window.height(), 0.1f, 10.0f);
	m_mvp.proj[1][1] *= -1;
	//m_renderer.on_resize(e.width(), e.height());
}

void Application::on_update() {
	static auto start_time = std::chrono::high_resolution_clock::now();
	auto current_time = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

	double time_point = glfwGetTime();

	static float delta = 0.68f;

	float time_diff = (float)(time_point - m_prev_time_point);

	m_mvp.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	//m_graphics_controller.buffer_update(m_uniform_buffer, &m_mvp);
	//m_vertex_buffer->update(vertices, sizeof(vertices));

	m_clear_color.r += delta * time_diff;
	m_clear_color.b += delta * time_diff;

	if (m_clear_color.r >= 1.0f) {
		m_clear_color.r = 1.0f;
		m_clear_color.b = 1.0f;
		delta = -0.5f;
	} else if (m_clear_color.r <= 0.0f) {
		m_clear_color.r = 0.0f;
		m_clear_color.b = 0.0f;
		delta = 0.5f;
	}
	
	m_prev_time_point = time_point;

	for (const auto& layer : m_layer_stack)
		layer->on_update();
}

void Application::on_render() {
	glm::vec4 clear_values[2] = { { 0.9f, 0.7f, 0.8f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f } };

	UniformSetId sets[2] = { m_uniform_set0, m_uniform_set1 };

	m_graphics_controller.draw_begin(m_framebuffer, clear_values, 2);
	m_graphics_controller.draw_bind_pipeline(m_hdr_pipeline);
	m_graphics_controller.draw_bind_vertex_buffer(m_vertex_buffer);
	m_graphics_controller.draw_bind_index_buffer(m_index_buffer, m_index_type);
	m_graphics_controller.draw_bind_uniform_sets(m_hdr_pipeline, sets, 2);
	m_graphics_controller.draw_draw_indexed(m_index_count);
	m_graphics_controller.draw_end();

	m_graphics_controller.draw_begin_for_screen(m_clear_color);
	//m_graphics_controller.draw_bind_pipeline(m_display_pipeline);
	m_graphics_controller.draw_end_for_screen();
	
	m_graphics_controller.end_frame();

	for (const auto& layer : m_layer_stack)
		layer->on_render();
}