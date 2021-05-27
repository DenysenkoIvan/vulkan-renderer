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

static std::unique_ptr<uint8_t[]> rgb_to_rgba(const uint8_t* data, size_t texel_count) {
	std::unique_ptr<uint8_t[]> image = std::make_unique<uint8_t[]>(texel_count * 4);

	for (size_t i = 0; i < texel_count; i += 3) {
		image[i] = data[i];
		image[i + 1] = data[i + 1];
		image[i + 2] = data[i + 2];
		image[i + 3] = 255;
	}

	return image;
}

static MagFilter gltf_mag_filter_to_mag_filter(int mag_filter) {
	if (mag_filter == 9728)
		return MagFilter::Nearest;
	else
		return MagFilter::Linear;
}

static MinFilter gltf_min_filter_to_min_filter(int min_filter) {
	if (min_filter == 9984)
		return MinFilter::NearestMipMapNearest;
	else if (min_filter == 9985)
		return MinFilter::LinearMipMapNearest;
	else if (min_filter == 9986)
		return MinFilter::NearestMipMapLinear;
	else
		return MinFilter::LinearMipMapLinear;
}

static Wrap gltf_wrap_to_wrap(int wrap) {
	if (wrap == 33071)
		return Wrap::ClampToEdge;
	else if (wrap == 33648)
		return Wrap::MirroredRepeat;
	else
		return Wrap::Repeat;
}

static std::unique_ptr<Node> load_gltf_node(Node* parent, tinygltf::Node& gltf_node, tinygltf::Model& gltf_model, std::vector<Vertex>& vertex_buffer, std::vector<uint32_t>& index_buffer) {
	std::unique_ptr<Node> node = std::make_unique<Node>(parent);

	if (gltf_node.translation.size() == 3)
		node->translation = glm::make_vec3(gltf_node.translation.data());
	if (gltf_node.rotation.size() == 4)
		node->rotation = glm::make_quat(gltf_node.rotation.data());
	if (gltf_node.scale.size() == 3)
		node->scale = glm::make_vec3(gltf_node.scale.data());
	if (gltf_node.matrix.size() == 16)
		node->matrix = glm::make_mat4(gltf_node.matrix.data());

	node->children.reserve(gltf_node.children.size());
	for (size_t i = 0; i < gltf_node.children.size(); i++)
		node->children.push_back(load_gltf_node(node.get(), gltf_model.nodes[gltf_node.children[i]], gltf_model, vertex_buffer, index_buffer));

	if (gltf_node.mesh <= -1) // Node does not contain mesh
		return node;

	const tinygltf::Mesh& gltf_mesh = gltf_model.meshes[gltf_node.mesh];
	node->primitives.reserve(gltf_mesh.primitives.size());
	for (size_t i = 0; i < gltf_mesh.primitives.size(); i++) {
		const tinygltf::Primitive& gltf_primitive = gltf_mesh.primitives[i];

		size_t first_index = index_buffer.size();
		uint32_t vertex_start = (uint32_t)vertex_buffer.size();
		size_t index_count = 0;
		size_t vertex_count = 0;
		bool has_indices = gltf_primitive.indices > -1;

		// Load vertex data
		const tinygltf::Accessor& pos_accessor = gltf_model.accessors[gltf_primitive.attributes.find("POSITION")->second];
		const tinygltf::BufferView& pos_view = gltf_model.bufferViews[pos_accessor.bufferView];
		
		vertex_count = pos_accessor.count;

		int pos_byte_stride = pos_accessor.ByteStride(pos_view) ? pos_accessor.ByteStride(pos_view) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
		int normal_byte_stride = 0;
		int tangent_byte_stride = 0;
		int uv0_byte_stride = 0;
		int uv1_byte_stride = 0;

		const float* pos_buffer = reinterpret_cast<const float*>(&(gltf_model.buffers[pos_view.buffer].data[pos_accessor.byteOffset + pos_view.byteOffset]));
		const float* normal_buffer = nullptr;
		const float* tangent_buffer = nullptr;
		const float* uv0_buffer = nullptr;
		const float* uv1_buffer = nullptr;

		if (gltf_primitive.attributes.find("NORMAL") != gltf_primitive.attributes.end()) {
			const tinygltf::Accessor& normal_accessor = gltf_model.accessors[gltf_primitive.attributes.find("NORMAL")->second];
			const tinygltf::BufferView& normal_view = gltf_model.bufferViews[normal_accessor.bufferView];
			normal_buffer = reinterpret_cast<const float*>(&(gltf_model.buffers[normal_view.buffer].data[normal_accessor.byteOffset + normal_view.byteOffset]));
			normal_byte_stride = normal_accessor.ByteStride(normal_view) ? normal_accessor.ByteStride(normal_view) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
		}

		if (gltf_primitive.attributes.find("TANGENT") != gltf_primitive.attributes.end()) {
			const tinygltf::Accessor& tangent_accessor = gltf_model.accessors[gltf_primitive.attributes.find("TANGENT")->second];
			const tinygltf::BufferView& tangent_view = gltf_model.bufferViews[tangent_accessor.bufferView];
			tangent_buffer = reinterpret_cast<const float*>(&(gltf_model.buffers[tangent_view.buffer].data[tangent_accessor.byteOffset + tangent_view.byteOffset]));
			tangent_byte_stride = tangent_accessor.ByteStride(tangent_view) ? tangent_accessor.ByteStride(tangent_view) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
		}
		// else
		//	continue; // skip primitives which don't have tangents

		if (gltf_primitive.attributes.find("TEXCOORD_0") != gltf_primitive.attributes.end()) {
			const tinygltf::Accessor& uv_accessor = gltf_model.accessors[gltf_primitive.attributes.find("TEXCOORD_0")->second];
			const tinygltf::BufferView& uv_view = gltf_model.bufferViews[uv_accessor.bufferView];
			uv0_buffer = reinterpret_cast<const float*>(&(gltf_model.buffers[uv_view.buffer].data[uv_accessor.byteOffset + uv_view.byteOffset]));
			uv0_byte_stride = uv_accessor.ByteStride(uv_view) ? (uv_accessor.ByteStride(uv_view) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
		}

		if (gltf_primitive.attributes.find("TEXCOORD_1") != gltf_primitive.attributes.end()) {
			const tinygltf::Accessor& uv_accessor = gltf_model.accessors[gltf_primitive.attributes.find("TEXCOORD_1")->second];
			const tinygltf::BufferView& uv_view = gltf_model.bufferViews[uv_accessor.bufferView];
			uv1_buffer = reinterpret_cast<const float*>(&(gltf_model.buffers[uv_view.buffer].data[uv_accessor.byteOffset + uv_view.byteOffset]));
			uv1_byte_stride = uv_accessor.ByteStride(uv_view) ? (uv_accessor.ByteStride(uv_view) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
		}

		vertex_buffer.reserve(vertex_buffer.size() + vertex_count);
		for (size_t i = 0; i < vertex_count; i++) {
			Vertex vertex{
				.pos = glm::make_vec3(&pos_buffer[i * pos_byte_stride]),
				.normal = glm::normalize(glm::vec3(normal_buffer ? glm::make_vec3(&normal_buffer[i * normal_byte_stride]) : glm::vec3(0.0f))),
				.tangent = glm::normalize(glm::vec4(tangent_buffer ? glm::make_vec4(&tangent_buffer[i * tangent_byte_stride]) : glm::vec4(0.0f))),
				.uv0 = uv0_buffer ? glm::make_vec2(&uv0_buffer[i * uv0_byte_stride]) : glm::vec2(0.0f),
				.uv1 = uv1_buffer ? glm::make_vec2(&uv1_buffer[i * uv1_byte_stride]) : glm::vec2(0.0f)
			};

			vertex_buffer.push_back(vertex);
		}

		// Load index data
		if (has_indices) {
			const tinygltf::Accessor& accessor = gltf_model.accessors[gltf_primitive.indices > -1 ? gltf_primitive.indices : 0];
			const tinygltf::BufferView& buffer_view = gltf_model.bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = gltf_model.buffers[buffer_view.buffer];

			const void* ptr = &(buffer.data[accessor.byteOffset + buffer_view.byteOffset]);
			
			index_count = accessor.count;

			index_buffer.reserve(index_buffer.size() + index_count);
			switch (accessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			{
				const uint32_t* buf = (const uint32_t*)ptr;
				for (size_t index = 0; index < index_count; index++)
					index_buffer.push_back(buf[index] + vertex_start);
			} break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
				const uint16_t* buf = (const uint16_t*)ptr;
				for (size_t index = 0; index < index_count; index++)
					index_buffer.push_back(buf[index] + vertex_start);
			} break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
				const uint8_t* buf = (const uint8_t*)ptr;
				for (size_t index = 0; index < index_count; index++)
					index_buffer.push_back(buf[index] + vertex_start);
			} break;
			default:
				throw std::runtime_error("Index component type not supported");
			}
		}

		Primitive primitive{
			.first_index = first_index,
			.index_count = index_count,
			.vertex_count = vertex_count,
			.material_id = (MaterialId)gltf_primitive.material,
			.has_indices = has_indices
		};

		node->primitives.push_back(primitive);
	}
	
	return node;
}

Model Application::load_gltf_model(const std::filesystem::path& filename) {
	if (!std::filesystem::exists(filename))
		throw std::runtime_error("GLTF file does not exist");

	tinygltf::TinyGLTF gltf_context;
	tinygltf::Model gltf_model;

	std::string warn, err;
	bool good = gltf_context.LoadASCIIFromFile(&gltf_model, &err, &warn, filename.string());

	if (!err.empty())
		throw std::runtime_error(err);

	if (!warn.empty())
		std::cout << warn << '\n';

	Model model;
	
	// Load images
	std::vector<std::unique_ptr<uint8_t[]>> raw_images;
	
	std::vector<ImageSpecs> images;
	images.reserve(gltf_model.images.size());

	std::vector<SamplerSpecs> samplers;
	samplers.reserve(gltf_model.samplers.size());

	std::vector<TextureSpecs> textures;
	textures.reserve(gltf_model.textures.size());

	std::vector<MaterialSpecs> materials;
	materials.reserve(gltf_model.materials.size());

	for (const auto& gltf_image : gltf_model.images) {
		ImageSpecs image{
			.width = (uint32_t)gltf_image.width,
			.height = (uint32_t)gltf_image.height,
			.data = gltf_image.image.data()
		};
	
		if (gltf_image.component == 3) {
			raw_images.push_back(rgb_to_rgba(gltf_image.image.data(), image.width * image.height));
			image.data = raw_images.back().get();
		}

		images.push_back(image);
	}

	// Create samplers
	for (const auto& gltf_sampler : gltf_model.samplers) {
		SamplerSpecs sampler{
			.mag_filter = gltf_mag_filter_to_mag_filter(gltf_sampler.magFilter),
			.min_filter = gltf_min_filter_to_min_filter(gltf_sampler.minFilter),
			.wrap_u = gltf_wrap_to_wrap(gltf_sampler.wrapS),
			.wrap_v = gltf_wrap_to_wrap(gltf_sampler.wrapT)
		};

		samplers.push_back(sampler);
	}
	// Default sampler
	samplers.push_back({
		.mag_filter = MagFilter::Linear,
		.min_filter = MinFilter::LinearMipMapLinear,
		.wrap_u = Wrap::Repeat,
		.wrap_v = Wrap::Repeat
	});

	// Create texture. Texture = image + sampler
	for (const auto& gltf_texture : gltf_model.textures) {
		TextureSpecs texture{
			.image_id = (uint32_t)gltf_texture.source
		};

		if (gltf_texture.sampler == -1)
			texture.sampler_id = (uint32_t)samplers.size() - 1; // Use default one

		textures.push_back(texture);
	}

	// Create materials
	for (auto& gltf_material : gltf_model.materials) {
		MaterialSpecs material{};

		if (gltf_material.values.find("baseColorTexture") != gltf_material.values.end()) {
			uint32_t uv_set = gltf_material.values["baseColorTexture"].TextureTexCoord();
			uint32_t texture_id = gltf_material.values["baseColorTexture"].TextureIndex();

			if (uv_set == 0 || uv_set == 1) {
				material.info.base_color_uv_set = uv_set;
				material.albedo_id = texture_id;
			}
		}
		if (gltf_material.values.find("metallicRoughnessTexture") != gltf_material.values.end()) {
			uint32_t uv_set = gltf_material.values["metallicRoughnessTexture"].TextureTexCoord();
			uint32_t texture_id = gltf_material.values["metallicRoughnessTexture"].TextureIndex();

			if (uv_set == 0 || uv_set == 1) {
				material.info.ao_rough_met_uv_set = uv_set;
				material.ao_rough_met_id = texture_id;
			}
		}
		if (gltf_material.additionalValues.find("normalTexture") != gltf_material.additionalValues.end()) {
			uint32_t uv_set = gltf_material.additionalValues["normalTexture"].TextureTexCoord();
			uint32_t texture_id = gltf_material.additionalValues["normalTexture"].TextureIndex();

			if (uv_set == 0 || uv_set == 1) {
				material.info.normals_uv_set = uv_set;
				material.normals_id = texture_id;
			}
		}
		if (gltf_material.additionalValues.find("occlusionTexture") != gltf_material.additionalValues.end()) {
			uint32_t uv_set = gltf_material.additionalValues["occlusionTexture"].TextureTexCoord();
			uint32_t texture_id = gltf_material.additionalValues["occlusionTexture"].TextureIndex();

			if ((uv_set == 0 || uv_set == 1) && texture_id == material.ao_rough_met_id && uv_set == material.info.ao_rough_met_uv_set)
				material.info.is_ao_in_rough_met = 1.0f;
		}
		if (gltf_material.additionalValues.find("emissiveTexture") != gltf_material.additionalValues.end()) {
			uint32_t uv_set = gltf_material.additionalValues["emissiveTexture"].TextureTexCoord();
			uint32_t texture_id = gltf_material.additionalValues["emissiveTexture"].TextureIndex();

			if (uv_set == 0 || uv_set == 1) {
				material.info.emissive_uv_set = uv_set;
				material.emissive_id = texture_id;
			}
		}

		if (gltf_material.values.find("baseColorFactor") != gltf_material.values.end())
			material.info.base_color_factor = glm::make_vec4(gltf_material.values["baseColorFactor"].ColorFactor().data());
		if (gltf_material.values.find("roughnessFactor") != gltf_material.values.end())
			material.info.roughness_factor = (float)gltf_material.values["roughnessFactor"].Factor();
		if (gltf_material.values.find("metallicFactor") != gltf_material.values.end())
			material.info.metallic_factor = (float)gltf_material.values["metallicFactor"].Factor();
		if (gltf_material.additionalValues.find("emissiveFactor") != gltf_material.additionalValues.end())
			material.info.emissive_factor = glm::vec4(glm::make_vec3(gltf_material.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0f);

		if (gltf_material.additionalValues.find("alphaMode") != gltf_material.additionalValues.end()) {
			tinygltf::Parameter param = gltf_material.additionalValues["alphaMode"];

			if (param.string_value == "MASK") {
				material.alpha_mode = AlphaMode::Mask;
				material.info.alpha_mask = 1.0f;

				if (gltf_material.additionalValues.find("alphaCutoff") != gltf_material.additionalValues.end())
					material.info.alpha_cutoff = (float)gltf_material.additionalValues["alphaCutoff"].Factor();
			} else if (param.string_value == "BLEND")
				material.alpha_mode = AlphaMode::Blend;
		}

		materials.push_back(material);
	}

	m_renderer.materials_create(
		images.data(), (uint32_t)images.size(),
		samplers.data(), (uint32_t)samplers.size(),
		textures.data(), (uint32_t)textures.size(),
		materials.data(), (uint32_t)materials.size()
	);

	// Load nodes and geometry
	std::vector<Vertex> vertex_buffer;
	std::vector<uint32_t> index_buffer;

	tinygltf::Scene& gltf_scene = gltf_model.scenes[gltf_model.defaultScene > -1 ? gltf_model.defaultScene : 0];
	for (size_t i = 0; i < gltf_scene.nodes.size(); i++) {
		tinygltf::Node& gltf_node = gltf_model.nodes[gltf_scene.nodes[i]];

		model.nodes.push_back(load_gltf_node(nullptr, gltf_node, gltf_model, vertex_buffer, index_buffer));
	}

	if (!vertex_buffer.empty())
		model.vertex_buffer_id = m_renderer.vertex_buffer_create(vertex_buffer.data(), vertex_buffer.size());
	if (!index_buffer.empty())
		model.index_buffer_id = m_renderer.index_buffer_create(index_buffer.data(), index_buffer.size());

	std::cout << "Vertex count: " << vertex_buffer.size() << '\n';
	std::cout << "Index count: " << index_buffer.size() << '\n';

	return model;
}

void Application::draw_node(const Node& node, const Model& model, const glm::mat4& matrix) {
	glm::mat4 new_matrix = matrix * node.local_matrix();

	for (const Primitive& primitive : node.primitives) {
		m_renderer.draw_primitive(
			new_matrix,
			model.vertex_buffer_id,
			model.index_buffer_id,
			primitive.first_index,
			primitive.index_count,
			primitive.vertex_count,
			primitive.material_id
		);
	}

	for (const auto& node : node.children)
		draw_node(*node, model, new_matrix);
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

	uint32_t resolution_coef = 1;
	m_renderer.set_resolution(resolution_coef * 1920, resolution_coef * 1080);
	
	//uint32_t shadow_map_resolution = 2048 * 4;
	//m_renderer.set_shadow_map_resolution(shadow_map_resolution, shadow_map_resolution);

	m_prev_mouse_x = props.width / 2;
	m_prev_mouse_y = props.height / 2;

	m_camera.front = glm::vec3(2.0f, 2.0f, 0.0f);
	m_camera.front = glm::normalize(m_camera.front);

	m_monitor_resolution = Window::get_monitor_resolution();
	m_camera.aspect_ratio = (float)m_monitor_resolution.width / m_monitor_resolution.height;
	m_camera.near = 0.1f;
	m_camera.far = 10'000.0f;

	glm::vec3 light_pos{ 5.0f, 15.0f, 5.0f };
	m_directional_light = {
		.color = glm::vec3(23.47f, 21.31f, 20.79f) / glm::vec3(8.0f),
		.pos = light_pos,
		.dir = glm::normalize(light_pos)
	};

	m_model = load_gltf_model("../assets/models/pony_cartoon/scene.gltf");

	int width = 0, height = 0;
	std::vector<uint8_t> skybox_pixels = load_cube_map("../assets/environment maps/lakeside.hdr", &width, &height);
	
	ImageSpecs skybox_texture{
		.width = (uint32_t)width,
		.height = (uint32_t)height,
		.data = skybox_pixels.data()
	};

	m_skybox = m_renderer.skybox_create(skybox_texture);
}

Application::~Application() {
	m_renderer.destroy();
}

void Application::on_event(Event& e) {
	EventDispatcher dispatcher(e);

	dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) { this->on_window_resize(e); });
	dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) { this->on_window_close(e); });
	dispatcher.dispatch<MouseMovedEvent>([this](MouseMovedEvent& e) { this->on_mouse_move(e); });
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

	float speed = delta_time * 10.0f;
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

	float sensetivity = 0.06f;
	delta_mouse_x *= sensetivity;
	delta_mouse_y *= sensetivity;

	m_camera.turn_left(-delta_mouse_x);
	m_camera.turn_up(delta_mouse_y);

	m_previous_time_step = time;
	m_prev_mouse_x = m_mouse_x;
	m_prev_mouse_y = m_mouse_y;
}

void Application::on_render() {
	m_renderer.begin_frame(m_camera, m_directional_light, nullptr, 0);

	for (const auto& node : m_model.nodes)
		draw_node(*node, m_model, glm::mat4(1.0f));

	m_renderer.draw_skybox(m_skybox);

	m_renderer.end_frame(m_window.width(), m_window.height());
}