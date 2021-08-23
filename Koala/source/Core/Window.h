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
	std::function<void(const Event&)> callback;
};

struct Resolution {
	uint32_t width = 0;
	uint32_t height = 0;
};

class Window {
public:
	Window() = default;
	~Window();

	static Resolution get_monitor_resolution();

	int width() const { return m_window_info.width; }
	int height() const { return m_window_info.height; }

	bool is_minimized();

	void initialize(const WindowProperties& window_porps);
	void on_update();

	GLFWwindow* get_GLFWwindow() const { return m_window; }
	VulkanContext* context() const { return m_window_info.context.get(); }

private:
	void init_window(const WindowProperties& window_props);
	
private:
	static void init_glfw();
	static void terminate_glfw();

	static int s_windows_created_count;
private:
	struct WindowInfo {
		std::function<void(const Event&)> callback;
		int width;
		int height;
		std::unique_ptr<VulkanContext> context;
	} m_window_info;
	GLFWwindow* m_window = nullptr;

};