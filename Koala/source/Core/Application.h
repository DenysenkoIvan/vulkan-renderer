#pragma once

#include <memory>

#include <Core/LayerStack.h>
#include <Core/Window.h>

#include <Event/ApplicationEvent.h>
#include <Event/KeyboardEvent.h>
#include <Event/MouseEvent.h>

#include <Renderer/VulkanGraphicsController.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

int main(int argc, char** argv);

struct ApplicationProperties {
	std::string app_name;
	uint32_t app_version = 0;
	uint32_t width = 800;
	uint32_t height = 480;
};

struct Vertex {
	glm::vec3 pos;
	glm::vec2 tex_pos;

	bool operator==(const Vertex& v) const {
		return pos == v.pos && tex_pos == v.tex_pos;
	}
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

	VulkanGraphicsController m_graphics_controller;
	glm::vec4 m_clear_color = { 1.0f, 0.0f, 1.0f, 1.0f };
	ImageId m_color_attachment;
	ImageId m_depth_attachment;
	RenderPassId m_render_pass;
	FramebufferId m_framebuffer;
	ShaderId m_hdr_shader;
	PipelineId m_hdr_pipeline;
	ShaderId m_display_shader;
	PipelineId m_display_pipeline;

	BufferId m_vertex_buffer;
	BufferId m_index_buffer;
	IndexType m_index_type;
	uint32_t m_index_count;
	BufferId m_uniform_buffer;
	ImageId m_texture;
	SamplerId m_sampler;
	UniformSetId m_uniform_set0;
	UniformSetId m_uniform_set1;
	
	struct MVP {
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
	};
	
	MVP m_mvp;
	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec2>()(vertex.tex_pos) << 1)) >> 1);
		}
	};
}