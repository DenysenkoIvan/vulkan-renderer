#include "Application.h"

#include <stb_image/stb_image.h>
#include <tiny_obj_loader/tiny_obj_loader.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

static std::vector<uint8_t> load_cube_map(std::string_view filename, int* x, int* y) {
	int channels = -1;
	int rows = -1;
	int cols = -1;

	stbi_uc* pixels = stbi_load(filename.data(), &rows, &cols, &channels, STBI_rgb_alpha);

	if (!pixels)
		throw std::runtime_error("Failed to load image");

	size_t map_size = rows * cols * channels;

	std::vector<uint8_t> storage;
	storage.reserve(map_size);

	size_t offset = rows / 6 * 4;
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < cols; j++) {
			for (int k = 0; k < offset; k++) {
				size_t o = j * rows * 4 + i * offset + k;
				storage.push_back(pixels[o]);
			}
		}
	}

	stbi_image_free(pixels);

	*x = rows;
	*y = cols;

	return storage;
}

Application::Application(const ApplicationProperties& props) {
	m_start_time_point = std::chrono::steady_clock::now();

	WindowProperties window_props;
	window_props.width = props.width;
	window_props.height = props.height;
	window_props.title = props.app_name;
	window_props.callback = ([this](Event& e) { this->on_event(e); });

	m_window.initialize(window_props);

	m_renderer.create(m_window.context());
	m_renderer.set_resolution(1280, 720);
	//uint32_t shadow_map_resolution = 2048;
	//m_renderer.set_shadow_map_resolution(shadow_map_resolution, shadow_map_resolution);

	m_prev_mouse_x = props.width / 2;
	m_prev_mouse_y = props.height / 2;

	m_camera.front = glm::vec3(2.0f, 2.0f, 0.0f);
	m_camera.front = glm::normalize(m_camera.front);

	m_monitor_resolution = Window::get_monitor_resolution();
	m_camera.aspect_ratio = (float)m_monitor_resolution.width / m_monitor_resolution.height;
	m_camera.near = 0.1f;
	m_camera.far = 1'000'000.0f;

	create_viking_room();

	glm::vec3 light_pos{ 5.0f, 5.0f, 5.0f };
	m_directional_light = {
		.eye = light_pos,
		.front = glm::vec3(0.0f) - light_pos,
		.up = glm::vec3(0.0f, 0.0f, -1.0f)
	};

	m_renderer.set_directional_light(m_directional_light);

	int width = 0, height = 0;
	std::vector<uint8_t> skybox_pixels = load_cube_map("../assets/environment maps/lakeside.hdr", &width, &height);

	TextureSpecification skybox_texture{
		.width = (uint32_t)width,
		.height = (uint32_t)height,
		.data = skybox_pixels.data()
	};

	SkyboxId skybox = m_renderer.skybox_create(skybox_texture);
}

void Application::create_viking_room() {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	
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
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}
	}

	int width = 0, height = 0, channels = 0;
	stbi_uc* pixels = stbi_load("../assets/models/viking_room.png", &width, &height, &channels, STBI_rgb_alpha);

	if (!pixels)
		throw std::runtime_error("Failed to load image");

	TextureSpecification texture_spec{
		.width = (uint32_t)width,
		.height = (uint32_t)height,
		.data = pixels
	};

	m_viking_room_mesh = m_renderer.mesh_create(vertices, indices, texture_spec);
	
	m_viking_room_pos = glm::vec3(2.0f, 2.0f, 0.0f);

	glm::mat4 model = glm::translate(glm::mat4(1.0f), m_viking_room_pos);
	m_renderer.mesh_update_model_matrix(m_viking_room_mesh, model);
}

Application::~Application() {
	m_renderer.destroy();
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
	m_camera.aspect_ratio = (float)e.width() / (float)e.height();
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

	for (const auto& layer : m_layer_stack)
		layer->on_update();

	m_previous_time_step = time;
	m_prev_mouse_x = m_mouse_x;
	m_prev_mouse_y = m_mouse_y;
}

void Application::on_render() {
	m_renderer.begin_frame(m_camera);

	m_renderer.draw_mesh(m_viking_room_mesh);
	m_renderer.draw_skybox(m_skybox);

	m_renderer.end_frame(m_window.width(), m_window.height());

	for (const auto& layer : m_layer_stack)
		layer->on_render();
}