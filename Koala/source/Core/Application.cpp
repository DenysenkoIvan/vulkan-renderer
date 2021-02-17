#include "Application.h"

#include <Renderer/VulkanContext.h>
#include <stb_image/stb_image.h>
#include <tiny_obj_loader/tiny_obj_loader.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

Application::Application(const ApplicationProperties& props) {
	m_start_time_point = std::chrono::steady_clock::now();

	WindowProperties window_props;
	window_props.width = props.width;
	window_props.height = props.height;
	window_props.title = props.app_name;
	window_props.callback = ([this](Event& e) { this->on_event(e); });

	m_window.initialize(window_props);
	m_graphics_controller.create(m_window.context());

	m_color_attachment_width = 1280;
	m_color_attachment_height = 720;

	m_monitor_resolution = Window::get_monitor_resolution();
	m_monitor_aspect_ratio = (float)m_monitor_resolution.width / m_monitor_resolution.height;

	m_prev_mouse_x = props.width / 2;
	m_prev_mouse_y = props.height / 2;

	m_camera.front = glm::vec3(2.0f, 2.0f, 0.0f);
	m_camera.front= glm::normalize(m_camera.front);

	m_square_vertex_count = 4;
	float vertices[4 * 4] = {
		-1.0f, -1.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f, 1.0f,
		 1.0f,  1.0f, 1.0f, 1.0f
	};

	m_square_index_count = 6;
	m_square_index_type = IndexType::Uint32;
	uint32_t indices[6] = {
		0, 1, 2,
		1, 2, 3
	};

	m_square_vertex_buffer = m_graphics_controller.vertex_buffer_create(vertices, 4 * m_square_vertex_count * sizeof(float));
	m_square_index_buffer = m_graphics_controller.index_buffer_create(indices, m_square_index_count * sizeof(uint32_t), m_square_index_type);

	m_color_attachment = m_graphics_controller.image_create(nullptr, ImageUsageColorAttachment | ImageUsageSampled, Format::RGBA32_SFloat, m_color_attachment_width, m_color_attachment_height);
	m_depth_attachment = m_graphics_controller.image_create(nullptr, ImageUsageDepthStencilAttachment, Format::D32_SFloat, m_color_attachment_width, m_color_attachment_height);

	std::array<RenderPassAttachment, 2> attachments{};
	attachments[0].format = Format::RGBA32_SFloat;
	attachments[0].usage = ImageUsageColorAttachment | ImageUsageSampled;
	attachments[0].initial_action = InitialAction::Clear;
	attachments[0].final_action = FinalAction::Store;
	attachments[1].format = Format::D32_SFloat;
	attachments[1].usage = ImageUsageDepthStencilAttachment;
	attachments[1].initial_action = InitialAction::Clear;
	attachments[1].final_action = FinalAction::DontCare;

	m_render_pass = m_graphics_controller.render_pass_create(attachments.data(), (uint32_t)attachments.size());
	
	ImageId ids[2] = { m_color_attachment, m_depth_attachment };

	m_framebuffer = m_graphics_controller.framebuffer_create(m_render_pass, ids, 2);

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

	std::array<PipelineDynamicStateFlags, 2> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR };

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
	hdr_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
	hdr_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();
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
	display_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
	display_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();

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
	
	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 0.0f));
	m_model_uniform_buffer = m_graphics_controller.uniform_buffer_create(&model, sizeof(glm::mat4));
	m_proj_view_uniform_buffer = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));
	
	m_texture = m_graphics_controller.image_create(pixels, ImageUsageSampled | ImageUsageTransferDst, Format::RGBA8_SRGB, width, height);
	
	SamplerInfo sampler_info{};

	m_sampler = m_graphics_controller.sampler_create(sampler_info);
	m_display_sampler = m_graphics_controller.sampler_create(sampler_info);

	std::vector<Uniform> uniform_set0;
	uniform_set0.reserve(2);
	std::vector<Uniform> uniform_set1;
	uniform_set1.reserve(1);
	
	Uniform proj_view_buffer;
	proj_view_buffer.type = UniformType::UniformBuffer;
	proj_view_buffer.binding = 0;
	proj_view_buffer.ids.push_back(m_proj_view_uniform_buffer);

	Uniform model_buffer;
	model_buffer.type = UniformType::UniformBuffer;
	model_buffer.binding = 1;
	model_buffer.ids.push_back(m_model_uniform_buffer);

	Uniform texture;
	texture.type = UniformType::CombinedImageSampler;
	texture.binding = 0;
	texture.ids.push_back(m_texture);
	texture.ids.push_back(m_sampler);

	uniform_set0.push_back(std::move(proj_view_buffer));
	uniform_set0.push_back(std::move(model_buffer));
	uniform_set1.push_back(std::move(texture));

	m_uniform_set0 = m_graphics_controller.uniform_set_create(m_hdr_shader, 0, uniform_set0);
	m_uniform_set1 = m_graphics_controller.uniform_set_create(m_hdr_shader, 1, uniform_set1);

	std::vector<Uniform> display_uniforms;
	Uniform display_texture_buffer;
	display_texture_buffer.type = UniformType::CombinedImageSampler;
	display_texture_buffer.binding = 0;
	display_texture_buffer.ids.push_back(m_color_attachment);
	display_texture_buffer.ids.push_back(m_display_sampler);

	display_uniforms.push_back(display_texture_buffer);

	m_display_uniform_set = m_graphics_controller.uniform_set_create(m_display_shader, 0, display_uniforms);
}

Application::~Application() {
	m_graphics_controller.destroy();
}

void Application::on_event(Event& e) {
	EventDispatcher dispatcher(e);

	dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) { this->on_window_resize(e); });
	dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) { this->on_window_close(e); });
	dispatcher.dispatch<MouseMovedEvent>([this](MouseMovedEvent& e) { this->on_mouse_move(e); });
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
	
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), m_monitor_aspect_ratio, 0.1f, 10000.0f);
	proj[1][1] *= -1;
	glm::mat4 proj_view = proj * m_camera.view_matrix();

	m_graphics_controller.buffer_update(m_proj_view_uniform_buffer, &proj_view);
}

void Application::on_mouse_move(MouseMovedEvent& e) {
	static bool first_mouse = true;
	if (first_mouse) {
		m_prev_mouse_x = (int)e.x();
		m_prev_mouse_y = (int)e.y();
		first_mouse = false;
	}

	m_mouse_x = (int)e.x();
	m_mouse_y = (int)e.y();
}

void Application::on_update() {
	auto current_time = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - m_start_time_point).count();

	float delta_time = time - m_previous_time_step;

	float speed = delta_time * 2.0f;
	if (glfwGetKey(m_window.get_GLFWwindow(), GLFW_KEY_W) == GLFW_PRESS)
		m_camera.move_forward(speed);
	else if (glfwGetKey(m_window.get_GLFWwindow(), GLFW_KEY_S) == GLFW_PRESS)
		m_camera.move_forward(-speed);
	else if (glfwGetKey(m_window.get_GLFWwindow(), GLFW_KEY_A) == GLFW_PRESS)
		m_camera.move_left(speed);
	else if (glfwGetKey(m_window.get_GLFWwindow(), GLFW_KEY_D) == GLFW_PRESS)
		m_camera.move_left(-speed);
		
	float delta_mouse_x = (float)m_mouse_x - m_prev_mouse_x;
	float delta_mouse_y = (float)m_mouse_y - m_prev_mouse_y;

	float sensetivity = 0.1f;
	delta_mouse_x *= sensetivity;
	delta_mouse_y *= sensetivity;

	m_camera.turn_left(-delta_mouse_x);
	m_camera.turn_up(delta_mouse_y);

	glm::mat4 proj = glm::perspective(glm::radians(45.0f), m_monitor_aspect_ratio, 0.0001f, 1000.0f);
	proj[1][1] *= -1;
	glm::mat4 proj_view = proj * m_camera.view_matrix();
	m_graphics_controller.buffer_update(m_proj_view_uniform_buffer, &proj_view);

	//glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	//m_graphics_controller.buffer_update(m_model_uniform_buffer, &model);

	for (const auto& layer : m_layer_stack)
		layer->on_update();

	m_previous_time_step = time;
	m_prev_mouse_x = m_mouse_x;
	m_prev_mouse_y = m_mouse_y;
}

void Application::on_render() {
	glm::vec4 clear_values[2] = { { 0.9f, 0.7f, 0.8f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f } };
	m_graphics_controller.draw_begin(m_framebuffer, clear_values, 2);
	
	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)m_color_attachment_width, (float)m_color_attachment_height, 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, m_color_attachment_width, m_color_attachment_height);
	
	UniformSetId sets[2] = { m_uniform_set0, m_uniform_set1 };
	m_graphics_controller.draw_bind_pipeline(m_hdr_pipeline);
	m_graphics_controller.draw_bind_vertex_buffer(m_vertex_buffer);
	m_graphics_controller.draw_bind_index_buffer(m_index_buffer, m_index_type);
	m_graphics_controller.draw_bind_uniform_sets(m_hdr_pipeline, sets, 2);
	m_graphics_controller.draw_draw_indexed(m_index_count);
	
	m_graphics_controller.draw_end();


	m_graphics_controller.draw_begin_for_screen(m_clear_color);
	
	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)m_window.width(), (float)m_window.height(), 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, m_window.width(), m_window.height());
	
	UniformSetId display_sets[1] = { m_display_uniform_set };
	m_graphics_controller.draw_bind_pipeline(m_display_pipeline);
	m_graphics_controller.draw_bind_vertex_buffer(m_square_vertex_buffer);
	m_graphics_controller.draw_bind_index_buffer(m_square_index_buffer, m_square_index_type);
	m_graphics_controller.draw_bind_uniform_sets(m_display_pipeline, display_sets, 1);
	m_graphics_controller.draw_draw_indexed(m_square_index_count);
	
	m_graphics_controller.draw_end_for_screen();
	

	m_graphics_controller.end_frame();

	for (const auto& layer : m_layer_stack)
		layer->on_render();
}