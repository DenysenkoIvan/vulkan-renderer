#include "Application.h"

#include <Renderer/VulkanContext.h>
#include <Renderer/Buffer.h>

Application::Application(const ApplicationProperties& props) {
	WindowProperties window_props;
	window_props.width = props.width;
	window_props.height = props.height;
	window_props.title = props.app_name;
	window_props.callback = ([this](Event& e) { this->on_event(e); });

	m_window.initialize(window_props);

	m_renderer.create(m_window.context());
}

Application::~Application() {
	m_renderer.destroy();
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
	m_renderer.on_resize(e.width(), e.height());
}

void Application::on_update() {
	double time_point = glfwGetTime();

	static float delta = 0.68f;

	float time_diff = (float)(time_point - m_prev_time_point);

	//std::cout << m_clear_value.color.float32[0] << '\n';

	m_clear_value.color.float32[0] += (delta * time_diff);
	m_clear_value.color.float32[2] += (delta * time_diff);

	if (m_clear_value.color.float32[0] >= 1.0f) {
		m_clear_value.color.float32[0] = 1.0f;
		m_clear_value.color.float32[2] = 1.0f;
		delta = 0.5f;
	} else if (m_clear_value.color.float32[0] <= 0) {
		m_clear_value.color.float32[0] = 0.0f;
		m_clear_value.color.float32[2] = 0.0f;
		delta = -0.5f;
	}

	if (m_clear_value.color.float32[0] >= 1.0f || m_clear_value.color.float32[0] <= 0.0f)
		delta = -delta;

	m_prev_time_point = time_point;

	// TODO: Delete these lines
	if (m_clear_value.color.float32[0] <= 0.05f) {
		BufferLayout layout = {
			{ Shader::DataType::Float3, "in_position" },
			{ Shader::DataType::Float3, "in_color" }
		};

		float vertices[] = {
			-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f,
			 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f,
			 0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f
		};

		VertexBuffer *vertex_buffer = new VertexBuffer(vertices, sizeof(vertices), layout);
	}

	for (const auto& layer : m_layer_stack)
		layer->on_update();
}

void Application::on_render() {
	m_renderer.clear_screen(m_clear_value);
	m_renderer.display();

	for (const auto& layer : m_layer_stack)
		layer->on_render();
}