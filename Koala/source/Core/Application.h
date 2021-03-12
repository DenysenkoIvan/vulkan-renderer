#pragma once

#include <chrono>
#include <filesystem>
#include <memory>

#include <Core/LayerStack.h>
#include <Core/Window.h>

#include <Event/ApplicationEvent.h>
#include <Event/KeyboardEvent.h>
#include <Event/MouseEvent.h>

#include <Renderer/Renderer.h>

int main(int argc, char** argv);

struct ApplicationProperties {
	std::string app_name;
	uint32_t app_version = 0;
	uint32_t width = 800;
	uint32_t height = 480;
};

class Application {
public:
	Application(const ApplicationProperties& props={});
	virtual ~Application();

	template<typename T, typename... Args>
	void push_layer(Args&& ...args) { m_layer_stack.push_layer(std::move(std::make_unique<T>(std::forward<Args>(args)...))); }

	void on_event(Event& e);

private:
	void run();

	void create_viking_room();

	void on_window_close(WindowCloseEvent& e);
	void on_window_resize(WindowResizeEvent& e);
	void on_mouse_move(MouseMovedEvent& e);
	void on_update();
	void on_render();

	friend int main(int argc, char** argv);

private:
	std::chrono::steady_clock::time_point m_start_time_point;
	float m_previous_time_step;
	int m_prev_mouse_x, m_prev_mouse_y;
	int m_mouse_x, m_mouse_y;
	
	ApplicationProperties m_app_properties;
	Window m_window;
	LayerStack m_layer_stack;
	bool m_running = true;
	double m_prev_time_point = 0.0;

	Renderer m_renderer;
	Camera m_camera;
	Camera m_directional_light;

	Resolution m_monitor_resolution;
	SkyboxId m_skybox;
	MeshId m_viking_room_mesh;
	glm::vec3 m_viking_room_pos;
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec2>()(vertex.tex_pos) << 1)) >> 1);
		}
	};
}