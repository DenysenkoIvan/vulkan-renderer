#include <Core/Window.h>
#include <Event/ApplicationEvent.h>
#include <Event/KeyboardEvent.h>
#include <Event/MouseEvent.h>

// TODO: Refactor
#include <Renderer/VulkanContext.h>

int Window::s_windows_created_count = 0;

Window::~Window() {
	terminate_window();

	s_windows_created_count--;
	if (s_windows_created_count == 0)
		terminate_glfw();

	m_window_info.context->destroy();

	m_window_info.context.release();
}

Resolution Window::get_monitor_resolution() {
	const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

	return { (uint32_t)mode->width, (uint32_t)mode->height };
}

bool Window::is_minimized() {
	int minimized = glfwGetWindowAttrib(m_window, GLFW_ICONIFIED);

	return minimized;
}

void Window::initialize(const WindowProperties& window_props) {
	if (s_windows_created_count == 0) {
		init_glfw();
		s_windows_created_count++;
	}

	init_window(window_props);

	m_window_info.context = std::make_unique<VulkanContext>();
	m_window_info.context->create(m_window);
}

void Window::on_update() {
	glfwPollEvents();
}

void Window::init_window(const WindowProperties& window_props) {
	m_window_info.width = window_props.width;
	m_window_info.height = window_props.height;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_window = glfwCreateWindow(m_window_info.width, m_window_info.height, window_props.title.data(), nullptr, nullptr);
	if (!m_window)
		throw std::runtime_error("Failed to create a window");

	glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	m_window_info.callback = window_props.callback;

	glfwSetWindowUserPointer(m_window, &m_window_info);

	glfwSetWindowCloseCallback(m_window, [](GLFWwindow *window) {
		WindowInfo& window_info = *(WindowInfo*)glfwGetWindowUserPointer(window);

		WindowCloseEvent e;
		window_info.callback(e);
	});

	glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
		WindowInfo& window_info = *(WindowInfo*)glfwGetWindowUserPointer(window);

		if (glfwGetWindowAttrib(window, GLFW_ICONIFIED))
			return;

		window_info.context->resize(width, height);

		window_info.width = width;
		window_info.height = height;

		WindowResizeEvent e(width, height);
		window_info.callback(e);
	});

	glfwSetKeyCallback(m_window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
		WindowInfo& window_info = *(WindowInfo*)glfwGetWindowUserPointer(window);

		switch (action) {
			case GLFW_PRESS:
			{
				KeyPressedEvent e(key, 0);
				window_info.callback(e);
				break;
			}
			case GLFW_RELEASE:
			{
				KeyReleasedEvent e(key);
				window_info.callback(e);
				break;
			}
			case GLFW_REPEAT:
			{
				KeyPressedEvent e(key, 1);
				window_info.callback(e);
				break;
			}
		}
	});

	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
		WindowInfo& window_info = *(WindowInfo*)glfwGetWindowUserPointer(window);

		MouseMovedEvent e(xpos, ypos);
		window_info.callback(e);
	});

	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
		WindowInfo& window_info = *(WindowInfo*)glfwGetWindowUserPointer(window);

		switch (action) {
			case GLFW_PRESS: {
				MouseButtonPressedEvent e(button);
				window_info.callback(e);
				break;
			}
			case GLFW_RELEASE: {
				MouseButtonReleasedEvent e(button);
				window_info.callback(e);
				break;
			}
		}
	});
}

void Window::terminate_window() {
	glfwDestroyWindow(m_window);
}

void Window::init_glfw() {
	glfwInit();

	glfwSetErrorCallback([](int error_code, const char* description) {
		// TODO: Log Error
		std::cout << description << '\n';
	});
}

void Window::terminate_glfw() {
	glfwTerminate();
}