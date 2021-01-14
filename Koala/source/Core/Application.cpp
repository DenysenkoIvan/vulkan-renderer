#include "Application.h"

#include <Renderer/VulkanContext.h>

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

	float vertices[] = {
		-0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
		 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
		 0.0f,  0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
		-0.5f,  0.5f, 0.7f, 0.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, 0.7f, 0.0f, 1.0f, 0.0f,
		 0.0f, -0.5f, 0.7f, 0.0f, 1.0f, 0.0f,
		-0.5f, -0.5f, 0.6f, 0.0f, 0.0f, 1.0f,
		-0.5f,  0.5f, 0.6f, 0.0f, 0.0f, 1.0f,
		 0.0f,  0.5f, 0.6f, 0.0f, 0.0f, 1.0f,
	};

	uint32_t indices1[] = {
		0, 1, 2
	};

	uint32_t indices2[] = {
		3, 4, 5
	};

	uint32_t indices3[] = {
		6, 7, 8
	};

	m_graphics_controller.create(m_window.context());

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

	m_shader = m_graphics_controller.shader_create(load_spv("../assets/shaders/vertex.spv"), load_spv("../assets/shaders/fragment.spv"));

	PipelineInfo pipeline_info{};
	pipeline_info.shader = m_shader;
	pipeline_info.assembly.topology = PrimitiveTopology::TriangleList;
	pipeline_info.assembly.restart_enable = false;
	pipeline_info.raster.depth_clamp_enable = false;
	pipeline_info.raster.rasterizer_discard_enable = false;
	pipeline_info.raster.polygon_mode = PolygonMode::Fill;
	pipeline_info.raster.cull_mode = CullMode::None;
	pipeline_info.raster.depth_bias_enable = false;
	pipeline_info.raster.line_width = 1.0f;

	m_pipeline = m_graphics_controller.pipeline_create(&pipeline_info);

	m_vertex_buffer = m_graphics_controller.vertex_buffer_create(vertices, sizeof(vertices));
	m_index_buffer = m_graphics_controller.index_buffer_create(indices1, sizeof(indices1), IndexType::Uint32);
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
	//m_renderer.on_resize(e.width(), e.height());
}

void Application::on_update() {
	double time_point = glfwGetTime();

	static float delta = 0.68f;

	float time_diff = (float)(time_point - m_prev_time_point);

	//float value = m_clear_value.color.float32[0];

	//float vertices[] = {
	//	-0.5f, -0.5f, 0.0f, value, 0.0f, value,
	//	 0.5f, -0.5f, 0.0f, value, 0.0f, value,
	//	-0.5f,  0.5f, 0.0f, value, 0.0f, value,
	//	 0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 1.0f
	//};

	//m_vertex_buffer->update(vertices, sizeof(vertices));

	//m_clear_value.color.float32[0] += (delta * time_diff);
	//m_clear_value.color.float32[2] += (delta * time_diff);
	//
	//if (m_clear_value.color.float32[0] >= 1.0f) {
	//	m_clear_value.color.float32[0] = 1.0f;
	//	m_clear_value.color.float32[2] = 1.0f;
	//	delta = 0.5f;
	//} else if (m_clear_value.color.float32[0] <= 0) {
	//	m_clear_value.color.float32[0] = 0.0f;
	//	m_clear_value.color.float32[2] = 0.0f;
	//	delta = -0.5f;
	//}
	//
	//if (m_clear_value.color.float32[0] >= 1.0f || m_clear_value.color.float32[0] <= 0.0f)
	//	delta = -delta;
	//
	//m_prev_time_point = time_point;

	for (const auto& layer : m_layer_stack)
		layer->on_update();
}

void Application::on_render() {
	//m_renderer.begin_frame();
	//m_renderer.clear_screen(m_clear_value);
	//
	//m_renderer.submit_geometry(*m_vertex_buffer, *m_index_buffer1);
	//m_renderer.submit_geometry(*m_vertex_buffer, *m_index_buffer2);
	//m_renderer.submit_geometry(*m_vertex_buffer, *m_index_buffer3);
	//
	//m_renderer.end_frame();

	m_graphics_controller.begin_frame();
	m_graphics_controller.end_frame();

	for (const auto& layer : m_layer_stack)
		layer->on_render();
}