#pragma once

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

	void on_window_close(WindowCloseEvent& e);
	void on_window_resize(WindowResizeEvent& e);
	void on_update();
	void on_render();

	friend int main(int argc, char** argv);

private:
	ApplicationProperties m_app_properties;
	Window m_window;
	LayerStack m_layer_stack;
	bool m_running = true;
	double m_prev_time_point = 0.0;

	Renderer m_renderer;
	std::shared_ptr<VertexBuffer> m_vertex_buffer;
	std::shared_ptr<IndexBuffer> m_index_buffer1;
	std::shared_ptr<IndexBuffer> m_index_buffer2;
	std::shared_ptr<IndexBuffer> m_index_buffer3;
	std::shared_ptr<Material> m_material;
	VkClearValue m_clear_value = { 0.1f, 0.0f, 0.1f, 1.0f };
};