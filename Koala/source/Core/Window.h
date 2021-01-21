#pragma once

#include <functional>
#include <memory>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <Event/Event.h>

#include <Renderer/VulkanContext.h>

struct WindowProperties {
	std::string_view title;
	uint32_t width, height;
	Event::event_handler_fn callback;
};

class Window {
public:
	Window() = default;
	~Window();

	int width();
	int height();

	bool is_minimized();

	void initialize(const WindowProperties& window_porps);
	void on_update();

	GLFWwindow* get_GLFWwindow() const { return m_window; }
	VulkanContext* context() const { return m_context.get(); }

private:
	void init_window(const WindowProperties& window_props);
	void terminate_window();

private:
	static void init_glfw();
	static void terminate_glfw();

	static int s_windows_created_count;
private:
	Event::event_handler_fn m_callback;
	GLFWwindow* m_window = nullptr;
	int m_width;
	int m_height;

	std::unique_ptr<VulkanContext> m_context;
};