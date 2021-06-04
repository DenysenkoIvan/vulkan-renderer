#pragma once

#include <chrono>
#include <filesystem>
#include <memory>

#include <Core/Window.h>

#include <Event/ApplicationEvent.h>
#include <Event/KeyboardEvent.h>
#include <Event/MouseEvent.h>

#include <Renderer/Renderer.h>

struct ApplicationProperties {
	std::string app_name;
	uint32_t app_version = 0;
	uint32_t width = 800;
	uint32_t height = 480;
};

struct Primitive {
	size_t first_index = 0;
	size_t index_count = 0;
	size_t vertex_count = 0;
	MaterialId material_id = -1;
	bool has_indices = false;
};

struct Node {
	Node* parent;
	std::vector<std::unique_ptr<Node>> children;
	std::vector<Primitive> primitives;
	glm::mat4 matrix = glm::mat4(1.0f);
	glm::vec3 translation = glm::vec3(0.0f);
	glm::vec3 scale = glm::vec3(1.0f);
	glm::quat rotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);

	Node(Node* parent) : parent(parent) {}

	glm::mat4 local_matrix() const {
		return glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
	}
};

struct Model {
	std::vector<std::unique_ptr<Node>> nodes;
	VertexBufferId vertex_buffer_id;
	IndexBufferId index_buffer_id;
};

class Application {
public:
	Application(const ApplicationProperties& props={});
	~Application();

	void run();
	void on_event(Event& e);

private:
	void on_window_close(WindowCloseEvent& e);
	void on_window_resize(WindowResizeEvent& e);
	void on_mouse_move(MouseMovedEvent& e);
	void on_update();
	void on_render();

	Model load_gltf_model(const std::filesystem::path& filename);
	void draw_node(const Node& node, const Model& model, const glm::mat4& matrix);

private:
	std::chrono::steady_clock::time_point m_start_time_point;
	float m_previous_time_step;
	int m_prev_mouse_x, m_prev_mouse_y;
	int m_mouse_x, m_mouse_y;
	
	ApplicationProperties m_app_properties;
	Window m_window;
	bool m_running = true;
	double m_prev_time_point = 0.0;

	Resolution m_monitor_resolution;
	Renderer m_renderer;
	Camera m_camera;
	Light m_directional_light;

	SkyboxId m_skybox;
	Model m_model;
};