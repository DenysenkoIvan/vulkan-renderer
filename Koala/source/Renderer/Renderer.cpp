#include "Renderer.h"
#include <Profile.h>

// TODO: Logging
#include <iostream>
#include <filesystem>
#include <fstream>

#include <stb_image/stb_image.h>

static std::vector<uint8_t> load_spv(const std::filesystem::path& path) {
	if (!std::filesystem::exists(path))
		throw std::runtime_error("Shader doesn't exist");

	size_t code_size = std::filesystem::file_size(path);
	size_t zeros_count = (4 - (code_size % 4)) % 4;

	std::vector<uint8_t> spv_code;
	spv_code.reserve(code_size + zeros_count);

	std::basic_ifstream<uint8_t> spv_file(path, std::ios::binary);

	spv_code.insert(spv_code.end(), std::istreambuf_iterator<uint8_t>(spv_file), std::istreambuf_iterator<uint8_t>());
	spv_code.insert(spv_code.end(), zeros_count, 0);

	return spv_code;
}

static Filter mag_filter_to_filter(MagFilter filter) {
	if (filter == MagFilter::Nearest)
		return Filter::Nearest;
	else if (filter == MagFilter::Linear)
		return Filter::Linear;
	else
		return Filter::Nearest;
}

static Filter min_filter_to_filter(MinFilter filter) {
	if (filter == MinFilter::NearestMipMapNearest)
		return Filter::Nearest;
	else if (filter == MinFilter::LinearMipMapNearest)
		return Filter::Linear;
	else if (filter == MinFilter::NearestMipMapLinear)
		return Filter::Nearest;
	else if (filter == MinFilter::LinearMipMapLinear)
		return Filter::Linear;
	else
		return Filter::Nearest;
}

static MipMapMode min_filter_to_mip_map_mode(MinFilter filter) {
	if (filter == MinFilter::NearestMipMapNearest)
		return MipMapMode::Nearest;
	else if (filter == MinFilter::LinearMipMapNearest)
		return MipMapMode::Nearest;
	else if (filter == MinFilter::NearestMipMapLinear)
		return MipMapMode::Linear;
	else if (filter == MinFilter::LinearMipMapLinear)
		return MipMapMode::Linear;
	else
		return MipMapMode::Nearest;
}

static SamplerAddressMode wrap_to_sampler_address_mode(Wrap wrap) {
	if (wrap == Wrap::ClampToEdge)
		return SamplerAddressMode::ClampToEdge;
	else if (wrap == Wrap::MirroredRepeat)
		return SamplerAddressMode::MirroredRepeat;
	else if (wrap == Wrap::Repeat)
		return SamplerAddressMode::Repeat;
	else
		return SamplerAddressMode::Repeat;
}

void Renderer::create(VulkanContext* context) {
	MY_PROFILE_FUNCTION();

	m_graphics_controller.create(context);

	// Create empty texture
	{
		int8_t zeroes[4] = { 0, 0, 0, 0 };
		ImageInfo empty_texture_info{
			.usage = ImageUsageTransferDst | ImageUsageColorSampled,
			.view_type = ImageViewType::TwoD,
			.format = Format::RGBA8_UNorm,
			.extent = { 1, 1, 1},
			.mip_levels = 1,
			.array_layers = 1
		};

		ImageSubresourceLayers subres{
			.aspect = VK_IMAGE_ASPECT_COLOR_BIT,
			.mip_level = 0,
			.base_array_layer = 0,
			.layer_count = 1
		};

		m_defaults.empty_texture.image = m_graphics_controller.image_create(empty_texture_info);
		m_graphics_controller.image_update(m_defaults.empty_texture.image, subres, { 0, 0, 0 }, { 1, 1, 1 }, { empty_texture_info.format, zeroes });
	
		SamplerInfo empty_texture_sampler_info{
			.mag_filter = Filter::Nearest,
			.min_filter = Filter::Nearest,
			.mip_map_mode = MipMapMode::Nearest
		};
		m_defaults.empty_texture.sampler = m_graphics_controller.sampler_create(empty_texture_sampler_info);
	}

	// Setup scene resources
	m_scene_info.gpu.view_pos = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::vec3));
	m_scene_info.gpu.projview_matrix = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));
	m_scene_info.gpu.projview_matrix_no_translation = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));

	// Create deferred render targets
	{
		m_deferred.albedo_info.usage = ImageUsageColorAttachment | ImageUsageColorSampled;
		m_deferred.albedo_info.view_type = ImageViewType::TwoD;
		m_deferred.albedo_info.format = Format::BGRA8_UNorm;
		m_deferred.albedo_info.extent.depth = 1;
		m_deferred.albedo_info.array_layers = 1;

		m_deferred.ao_rough_met_info.usage = ImageUsageColorAttachment | ImageUsageColorSampled;
		m_deferred.ao_rough_met_info.view_type = ImageViewType::TwoD;
		m_deferred.ao_rough_met_info.format = Format::BGRA8_UNorm;
		m_deferred.ao_rough_met_info.extent.depth = 1;
		m_deferred.ao_rough_met_info.array_layers = 1;

		m_deferred.normals_info.usage = ImageUsageColorAttachment | ImageUsageColorSampled;
		m_deferred.normals_info.view_type = ImageViewType::TwoD;
		m_deferred.normals_info.format = Format::RGBA8_SNorm;
		m_deferred.normals_info.extent.depth = 1;
		m_deferred.normals_info.array_layers = 1;

		m_deferred.emissive_info.usage = ImageUsageColorAttachment | ImageUsageColorSampled;
		m_deferred.emissive_info.view_type = ImageViewType::TwoD;
		m_deferred.emissive_info.format = Format::RGBA8_UNorm;
		m_deferred.emissive_info.extent.depth = 1;
		m_deferred.emissive_info.array_layers = 1;

		m_deferred.depth_stencil_info.usage = ImageUsageDepthStencilAttachment | ImageUsageDepthStencilReadOnly | ImageUsageDepthSampled;
		m_deferred.depth_stencil_info.view_type = ImageViewType::TwoD;
		m_deferred.depth_stencil_info.format = Format::D24_UNorm_S8_UInt;
		m_deferred.depth_stencil_info.extent.depth = 1;
		m_deferred.depth_stencil_info.array_layers = 1;

		m_deferred.composition_info.usage = ImageUsageColorAttachment | ImageUsageColorSampled;
		m_deferred.composition_info.view_type = ImageViewType::TwoD;
		m_deferred.composition_info.format = Format::RGBA16_SFloat;
		m_deferred.composition_info.extent.depth = 1;
		m_deferred.composition_info.array_layers = 1;

		// G pass
		std::array<RenderPassAttachment, 5> g_pass_attachments{};
		// Albedo
		g_pass_attachments[0].previous_usage = ImageUsageColorSampled;
		g_pass_attachments[0].current_usage = ImageUsageColorAttachment;
		g_pass_attachments[0].next_usage = ImageUsageColorSampled;
		g_pass_attachments[0].format = m_deferred.albedo_info.format;
		g_pass_attachments[0].initial_action = InitialAction::Clear;
		g_pass_attachments[0].final_action = FinalAction::Store;
		// AO-Rough-Met
		g_pass_attachments[1].previous_usage = ImageUsageColorSampled;
		g_pass_attachments[1].current_usage = ImageUsageColorAttachment;
		g_pass_attachments[1].next_usage = ImageUsageColorSampled;
		g_pass_attachments[1].format = m_deferred.ao_rough_met_info.format;
		g_pass_attachments[1].initial_action = InitialAction::Clear;
		g_pass_attachments[1].final_action = FinalAction::Store;
		// Normals
		g_pass_attachments[2].previous_usage = ImageUsageColorSampled;
		g_pass_attachments[2].current_usage = ImageUsageColorAttachment;
		g_pass_attachments[2].next_usage = ImageUsageColorSampled;
		g_pass_attachments[2].format = m_deferred.normals_info.format;
		g_pass_attachments[2].initial_action = InitialAction::Clear;
		g_pass_attachments[2].final_action = FinalAction::Store;
		// Emissive
		g_pass_attachments[3].previous_usage = ImageUsageColorSampled;
		g_pass_attachments[3].current_usage = ImageUsageColorAttachment;
		g_pass_attachments[3].next_usage = ImageUsageColorSampled;
		g_pass_attachments[3].format = m_deferred.emissive_info.format;
		g_pass_attachments[3].initial_action = InitialAction::Clear;
		g_pass_attachments[3].final_action = FinalAction::Store;
		// Depth-Stencil
		g_pass_attachments[4].previous_usage = ImageUsageDepthStencilReadOnly;
		g_pass_attachments[4].current_usage = ImageUsageDepthStencilAttachment;
		g_pass_attachments[4].next_usage = ImageUsageDepthStencilReadOnly;
		g_pass_attachments[4].format = m_deferred.depth_stencil_info.format;
		g_pass_attachments[4].initial_action = InitialAction::Clear;
		g_pass_attachments[4].final_action = FinalAction::Store;
		g_pass_attachments[4].stencil_initial_action = InitialAction::Clear;
		g_pass_attachments[4].stencil_final_action = FinalAction::Store;

		m_deferred.g_pass = m_graphics_controller.render_pass_create(g_pass_attachments.data(), (uint32_t)g_pass_attachments.size());

		std::array<RenderPassAttachment, 2> composition_attachments{};
		composition_attachments[0].previous_usage = ImageUsageColorSampled;
		composition_attachments[0].current_usage = ImageUsageColorAttachment;
		composition_attachments[0].next_usage = ImageUsageColorSampled;
		composition_attachments[0].format = m_deferred.composition_info.format;
		composition_attachments[0].initial_action = InitialAction::Clear;
		composition_attachments[0].final_action = FinalAction::Store;
		composition_attachments[1].previous_usage = ImageUsageDepthStencilReadOnly;
		composition_attachments[1].current_usage = ImageUsageDepthStencilReadOnly | ImageUsageDepthSampled;
		composition_attachments[1].next_usage = ImageUsageDepthStencilAttachment;
		composition_attachments[1].format = m_deferred.depth_stencil_info.format;
		composition_attachments[1].initial_action = InitialAction::Load;
		composition_attachments[1].final_action = FinalAction::Store;

		m_deferred.composition_pass = m_graphics_controller.render_pass_create(composition_attachments.data(), (uint32_t)composition_attachments.size());
	}

	// Create G pipeline
	{
		auto vert_spv = load_spv("../assets/shaders/g_pass.vert.spv");
		auto frag_spv = load_spv("../assets/shaders/g_pass.frag.spv");

		std::array<ShaderStage, 2> shader_stages;
		shader_stages[0] = {
			.stage = ShaderStageVertex,
			.spv = vert_spv.data(),
			.spv_size = vert_spv.size()
		};
		shader_stages[1] = {
			.stage = ShaderStageFragment,
			.spv = frag_spv.data(),
			.spv_size = frag_spv.size()
		};

		m_g_pipeline.shader = m_graphics_controller.shader_create(shader_stages.data(), (uint32_t)shader_stages.size());

		std::array<PipelineDynamicStateFlags, 3> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR, DYNAMIC_STATE_STENCIL_REFERENCE };

		std::array<ColorBlendAttachmentState, 4> blend_attachments{};
		blend_attachments[0].blend_enable = false;
		blend_attachments[0].color_write_mask = ColorComponentR | ColorComponentG | ColorComponentB | ColorComponentA;
		blend_attachments[1].blend_enable = false;
		blend_attachments[1].color_write_mask = ColorComponentR | ColorComponentG | ColorComponentB | ColorComponentA;
		blend_attachments[2].blend_enable = false;
		blend_attachments[2].color_write_mask = ColorComponentR | ColorComponentG | ColorComponentB | ColorComponentA;
		blend_attachments[3].blend_enable = false;
		blend_attachments[3].color_write_mask = ColorComponentR | ColorComponentG | ColorComponentB | ColorComponentA;

		PipelineInfo g_pipeline_info{};
		g_pipeline_info.shader_id = m_g_pipeline.shader;
		g_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
		g_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();
		g_pipeline_info.depth_stencil.depth_test_enable = true;
		g_pipeline_info.depth_stencil.depth_write_enable = true;
		g_pipeline_info.depth_stencil.stencil_test_enable = true;
		g_pipeline_info.depth_stencil.depth_compare_op = CompareOp::Less;
		g_pipeline_info.color_blend.attachment_count = (uint32_t)blend_attachments.size();
		g_pipeline_info.color_blend.attachments = blend_attachments.data();
		g_pipeline_info.render_pass_id = m_deferred.g_pass;

		m_g_pipeline.pipeline = m_graphics_controller.pipeline_create(g_pipeline_info);

		UniformInfo g_pipeline_uniform_set_0;
		g_pipeline_uniform_set_0.type = UniformType::UniformBuffer;
		g_pipeline_uniform_set_0.binding = 0;
		g_pipeline_uniform_set_0.ids = &m_scene_info.gpu.projview_matrix;
		g_pipeline_uniform_set_0.id_count = 1;

		m_g_pipeline.uniform_set_0 = m_graphics_controller.uniform_set_create(m_g_pipeline.shader, 0, &g_pipeline_uniform_set_0, 1);
	}

	// Create light pipeline
	{
		auto vert_spv = load_spv("../assets/shaders/present.vert.spv");
		auto frag_spv = load_spv("../assets/shaders/lightning.frag.spv");

		std::array<ShaderStage, 2> shader_stages;
		shader_stages[0] = {
			.stage = ShaderStageVertex,
			.spv = vert_spv.data(),
			.spv_size = vert_spv.size()
		};
		shader_stages[1] = {
			.stage = ShaderStageFragment,
			.spv = frag_spv.data(),
			.spv_size = frag_spv.size()
		};

		m_light_pipeline.shader = m_graphics_controller.shader_create(shader_stages.data(), (uint32_t)shader_stages.size());
		std::array<PipelineDynamicStateFlags, 3> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR, DYNAMIC_STATE_STENCIL_REFERENCE };

		std::array<ColorBlendAttachmentState, 1> light_attachments{};
		light_attachments[0].blend_enable = false;
		light_attachments[0].color_write_mask = ColorComponentR | ColorComponentG | ColorComponentB | ColorComponentA;

		PipelineInfo light_pipeline_info{};
		light_pipeline_info.shader_id = m_light_pipeline.shader;
		light_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
		light_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();
		light_pipeline_info.depth_stencil.depth_test_enable = false;
		light_pipeline_info.depth_stencil.depth_write_enable = false;
		light_pipeline_info.depth_stencil.stencil_test_enable = true;
		light_pipeline_info.depth_stencil.front.pass_op = StencilOp::Keep;
		light_pipeline_info.depth_stencil.front.compare_op = CompareOp::Equal;
		light_pipeline_info.depth_stencil.back.pass_op = StencilOp::Keep;
		light_pipeline_info.depth_stencil.back.compare_op = CompareOp::Equal;
		light_pipeline_info.color_blend.attachment_count = (uint32_t)light_attachments.size();
		light_pipeline_info.color_blend.attachments = light_attachments.data();
		light_pipeline_info.render_pass_id = m_deferred.composition_pass;

		m_light_pipeline.pipeline = m_graphics_controller.pipeline_create(light_pipeline_info);

		SamplerInfo sampler_info{
			.mag_filter = Filter::Nearest,
			.min_filter = Filter::Nearest,
			.mip_map_mode = MipMapMode::Nearest
		};

		m_light_pipeline.sampler = m_graphics_controller.sampler_create(sampler_info);
	}

	// Create blend pipeline
	{
		auto vert_spv = load_spv("../assets/shaders/blend.vert.spv");
		auto frag_spv = load_spv("../assets/shaders/blend.frag.spv");

		std::array<ShaderStage, 2> shader_stages;
		shader_stages[0] = {
			.stage = ShaderStageVertex,
			.spv = vert_spv.data(),
			.spv_size = vert_spv.size()
		};
		shader_stages[1] = {
			.stage = ShaderStageFragment,
			.spv = frag_spv.data(),
			.spv_size = frag_spv.size()
		};

		m_blend_pipeline.shader = m_graphics_controller.shader_create(shader_stages.data(), (uint32_t)shader_stages.size());

		std::array<PipelineDynamicStateFlags, 2> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR };

		std::array<ColorBlendAttachmentState, 1> blend_attachments{};
		blend_attachments[0].blend_enable = true;
		blend_attachments[0].src_color_blend_factor = BlendFactor::SrcAlpha;
		blend_attachments[0].dst_color_blend_factor = BlendFactor::OneMinusSrcAlpha;
		blend_attachments[0].color_blend_op = BlendOp::Add;
		blend_attachments[0].src_alpha_blend_factor = BlendFactor::One;
		blend_attachments[0].dst_alpha_blend_factor = BlendFactor::Zero;
		blend_attachments[0].color_write_mask = ColorComponentR | ColorComponentG | ColorComponentB | ColorComponentA;

		PipelineInfo blend_pipeline_info{};
		blend_pipeline_info.shader_id = m_blend_pipeline.shader;
		blend_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
		blend_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();
		blend_pipeline_info.color_blend.attachment_count = (uint32_t)blend_attachments.size();
		blend_pipeline_info.color_blend.attachments = blend_attachments.data();
		blend_pipeline_info.depth_stencil.depth_test_enable = true;
		blend_pipeline_info.depth_stencil.depth_write_enable = false;
		blend_pipeline_info.render_pass_id = m_deferred.composition_pass;

		m_blend_pipeline.pipeline = m_graphics_controller.pipeline_create(blend_pipeline_info);

		m_blend_pipeline.uniform_buffer = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(LightInfo));

		std::array<UniformInfo, 2> blend_pipeline_uniform_set_0;
		blend_pipeline_uniform_set_0[0].type = UniformType::UniformBuffer;
		blend_pipeline_uniform_set_0[0].binding = 0;
		blend_pipeline_uniform_set_0[0].ids = &m_scene_info.gpu.projview_matrix;
		blend_pipeline_uniform_set_0[0].id_count = 1;
		blend_pipeline_uniform_set_0[1].type = UniformType::UniformBuffer;
		blend_pipeline_uniform_set_0[1].binding = 1;
		blend_pipeline_uniform_set_0[1].ids = &m_blend_pipeline.uniform_buffer;
		blend_pipeline_uniform_set_0[1].id_count = 1;

		m_blend_pipeline.uniform_set_0 = m_graphics_controller.uniform_set_create(m_blend_pipeline.shader, 0, blend_pipeline_uniform_set_0.data(), (uint32_t)blend_pipeline_uniform_set_0.size());
	}

	// Create skybox pipeline
	{
		auto vert_spv = load_spv("../assets/shaders/skybox.vert.spv");
		auto frag_spv = load_spv("../assets/shaders/skybox.frag.spv");

		std::array<ShaderStage, 2> shader_stages;
		shader_stages[0] = {
			.stage = ShaderStageVertex,
			.spv = vert_spv.data(),
			.spv_size = vert_spv.size()
		};
		shader_stages[1] = {
			.stage = ShaderStageFragment,
			.spv = frag_spv.data(),
			.spv_size = frag_spv.size()
		};

		m_skybox_pipeline.shader = m_graphics_controller.shader_create(shader_stages.data(), (uint32_t)shader_stages.size());

		std::array<ColorBlendAttachmentState, 1> skybox_attachments{};
		skybox_attachments[0].blend_enable = false;
		skybox_attachments[0].color_write_mask = ColorComponentR | ColorComponentG | ColorComponentB | ColorComponentA;

		std::array<PipelineDynamicStateFlags, 2> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR };

		PipelineInfo skybox_pipeline_info{};
		skybox_pipeline_info.shader_id = m_skybox_pipeline.shader;
		skybox_pipeline_info.depth_stencil.depth_test_enable = true;
		skybox_pipeline_info.depth_stencil.depth_write_enable = false;
		skybox_pipeline_info.color_blend.attachments = skybox_attachments.data();
		skybox_pipeline_info.color_blend.attachment_count = (uint32_t)skybox_attachments.size();
		skybox_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();
		skybox_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
		skybox_pipeline_info.render_pass_id = m_deferred.composition_pass;

		m_skybox_pipeline.pipeline = m_graphics_controller.pipeline_create(skybox_pipeline_info);

		SamplerInfo sampler_info{
			.mag_filter = Filter::Linear,
			.min_filter = Filter::Linear,
			.mip_map_mode = MipMapMode::Linear,
			.anisotropy_enable = true,
			.max_anisotropy = 4.0f
		};

		m_skybox_pipeline.sampler = m_graphics_controller.sampler_create(sampler_info);

		UniformInfo skybox_uniform{
			.type = UniformType::UniformBuffer,
			.binding = 0,
			.ids = &m_scene_info.gpu.projview_matrix_no_translation,
			.id_count = 1
		};

		m_skybox_pipeline.uniform_set_0 = m_graphics_controller.uniform_set_create(m_skybox_pipeline.shader, 0, &skybox_uniform, 1);
	}

	// Create coord system pipeline
	{
		auto vert_spv = load_spv("../assets/shaders/coord_system.vert.spv");
		auto frag_spv = load_spv("../assets/shaders/coord_system.frag.spv");

		std::array<ShaderStage, 2> shader_stages;
		shader_stages[0] = {
			.stage = ShaderStageVertex,
			.spv = vert_spv.data(),
			.spv_size = vert_spv.size()
		};
		shader_stages[1] = {
			.stage = ShaderStageFragment,
			.spv = frag_spv.data(),
			.spv_size = frag_spv.size()
		};

		m_coord_system_pipeline.shader = m_graphics_controller.shader_create(shader_stages.data(), (uint32_t)shader_stages.size());

		std::array<PipelineDynamicStateFlags, 3> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR, DYNAMIC_STATE_LINE_WIDTH };

		std::array<ColorBlendAttachmentState, 1> coord_system_attachments{};
		coord_system_attachments[0].blend_enable = false;
		coord_system_attachments[0].color_write_mask = ColorComponentR | ColorComponentG | ColorComponentB | ColorComponentA;

		PipelineInfo coord_system_pipeline_info{};
		coord_system_pipeline_info.shader_id = m_coord_system_pipeline.shader;
		coord_system_pipeline_info.assembly.topology = PrimitiveTopology::LineList;
		coord_system_pipeline_info.depth_stencil.depth_test_enable = true;
		coord_system_pipeline_info.depth_stencil.depth_write_enable = false;
		coord_system_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
		coord_system_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();
		coord_system_pipeline_info.color_blend.attachment_count = (uint32_t)coord_system_attachments.size();
		coord_system_pipeline_info.color_blend.attachments = coord_system_attachments.data();
		coord_system_pipeline_info.render_pass_id = m_deferred.composition_pass;

		m_coord_system_pipeline.pipeline = m_graphics_controller.pipeline_create(coord_system_pipeline_info);

		float offset = 1000.0f;
		float vertices[] = {
			-offset,   0.0f,    0.0f, 0.5f, 0.0f, 0.0f,
			 offset,   0.0f,    0.0f, 1.0f, 0.0f, 0.0f,
			 0.0f,  -offset,    0.0f, 0.0f, 0.5f, 0.0f,
			 0.0f,   offset,    0.0f, 0.0f, 1.0f, 0.0f,
			 0.0f,     0.0f, -offset, 0.0f, 0.0f, 0.5f,
			 0.0f,     0.0f,  offset, 0.0f, 0.0f, 1.0f
		};

		m_coord_system_pipeline.vertex_buffer = m_graphics_controller.vertex_buffer_create(vertices, sizeof(vertices));

		UniformInfo coord_system_uniform{
			.type = UniformType::UniformBuffer,
			.binding = 0,
			.ids = &m_scene_info.gpu.projview_matrix,
			.id_count = 1
		};

		m_coord_system_pipeline.uniform_set_0 = m_graphics_controller.uniform_set_create(m_coord_system_pipeline.shader, 0, &coord_system_uniform, 1);
	}

	// Create present pipeline
	{
		auto vert_spv = load_spv("../assets/shaders/present.vert.spv");
		auto frag_spv = load_spv("../assets/shaders/present.frag.spv");

		std::array<ShaderStage, 2> shader_stages;
		shader_stages[0] = {
			.stage = ShaderStageVertex,
			.spv = vert_spv.data(),
			.spv_size = vert_spv.size()
		};
		shader_stages[1] = {
			.stage = ShaderStageFragment,
			.spv = frag_spv.data(),
			.spv_size = frag_spv.size()
		};

		m_present_pipeline.shader = m_graphics_controller.shader_create(shader_stages.data(), (uint32_t)shader_stages.size());

		std::array<PipelineDynamicStateFlags, 2> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR };

		std::array<ColorBlendAttachmentState, 1> present_attachments{};
		present_attachments[0].blend_enable = false;
		present_attachments[0].color_write_mask = ColorComponentR | ColorComponentG | ColorComponentB | ColorComponentA;

		PipelineInfo present_pipeline_info{};
		present_pipeline_info.shader_id = m_present_pipeline.shader;
		present_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
		present_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();
		present_pipeline_info.depth_stencil.depth_test_enable = false;
		present_pipeline_info.depth_stencil.depth_write_enable = false;
		present_pipeline_info.color_blend.attachment_count = (uint32_t)present_attachments.size();
		present_pipeline_info.color_blend.attachments = present_attachments.data();

		SamplerInfo sampler_info{
			.mag_filter = Filter::Linear,
			.min_filter = Filter::Linear
		};

		m_present_pipeline.diff_res_sampler = m_graphics_controller.sampler_create(sampler_info);

		sampler_info.mag_filter = Filter::Nearest;
		sampler_info.min_filter = Filter::Nearest;

		m_present_pipeline.same_res_sampler = m_graphics_controller.sampler_create(sampler_info);

		m_present_pipeline.pipeline = m_graphics_controller.pipeline_create(present_pipeline_info);
	}

	// Create generate cubemap pipeline
	{
		RenderPassAttachment gen_cubemap_attachment{
			.previous_usage = ImageUsageTransferSrc,
			.current_usage = ImageUsageColorAttachment,
			.next_usage = ImageUsageTransferSrc,
			.format = Format::RGBA16_SFloat,
			.initial_action = InitialAction::Clear,
			.final_action = FinalAction::Store
		};

		m_gen_cubemap_pipeline.render_pass = m_graphics_controller.render_pass_create(&gen_cubemap_attachment, 1);

		auto vert_spv = load_spv("../assets/shaders/equirect_to_cubemap.vert.spv");
		auto frag_spv = load_spv("../assets/shaders/equirect_to_cubemap.frag.spv");

		std::array<ShaderStage, 2> shader_stages;
		shader_stages[0] = {
			.stage = ShaderStageVertex,
			.spv = vert_spv.data(),
			.spv_size = vert_spv.size()
		};
		shader_stages[1] = {
			.stage = ShaderStageFragment,
			.spv = frag_spv.data(),
			.spv_size = frag_spv.size()
		};

		m_gen_cubemap_pipeline.shader = m_graphics_controller.shader_create(shader_stages.data(), (uint32_t)shader_stages.size());

		ColorBlendAttachmentState blend_attachment{};
		blend_attachment.blend_enable = false;
		blend_attachment.color_write_mask = ColorComponentR | ColorComponentG | ColorComponentB | ColorComponentA;

		std::array<PipelineDynamicStateFlags, 2> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR };

		PipelineInfo gen_cubemap_pi{};
		gen_cubemap_pi.shader_id = m_gen_cubemap_pipeline.shader;
		gen_cubemap_pi.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
		gen_cubemap_pi.dynamic_states.dynamic_states = dynamic_states.data();
		gen_cubemap_pi.color_blend.attachments = &blend_attachment;
		gen_cubemap_pi.color_blend.attachment_count = 1;
		gen_cubemap_pi.render_pass_id = m_gen_cubemap_pipeline.render_pass;

		m_gen_cubemap_pipeline.pipeline = m_graphics_controller.pipeline_create(gen_cubemap_pi);

		SamplerInfo sampler_info{
			.anisotropy_enable = true,
			.max_anisotropy = 16.0f
		};

		m_gen_cubemap_pipeline.sampler = m_graphics_controller.sampler_create(sampler_info);
	}

	// Create default shapes
	{ // Square
		float vertices[4 * 4] = {
			-1.0f, -1.0f, 0.0f, 0.0f,
			 1.0f, -1.0f, 1.0f, 0.0f,
			-1.0f,  1.0f, 0.0f, 1.0f,
			 1.0f,  1.0f, 1.0f, 1.0f
		};

		m_square.index_count = 6;
		m_square.index_type = IndexType::Uint32;
		uint32_t indices[6] = {
			0, 1, 2,
			1, 2, 3
		};

		m_square.vertex_buffer = m_graphics_controller.vertex_buffer_create(vertices, sizeof(vertices));
		m_square.index_buffer = m_graphics_controller.index_buffer_create(indices, m_square.index_count * sizeof(uint32_t), m_square.index_type);
	}

	// Create 1 * 1 * 1 box
	{
		float vertices[36 * 3] = {
			// Near plane
			-1.0f, -1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,

			// Far plane
			-1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f,

			// Left plane
			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f, -1.0f,

			// Right plane
			1.0f, -1.0f, -1.0f,
			1.0f, -1.0f,  1.0f,
			1.0f,  1.0f,  1.0f,
			1.0f,  1.0f, -1.0f,

			// Bottom plane
			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f, -1.0f,

			// Top plane
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f, -1.0f
		};

		m_box.index_count = 36;
		m_box.index_type = IndexType::Uint32;
		uint32_t indices[] = {
			 0,  1,  2,  0,  2,  3,
			 4,  5,  6,  4,  6,  7,
			 8,  9, 10,  8, 10, 11,
			12, 13, 14, 12, 14, 15,
			16, 17, 18, 16, 18, 19,
			20, 21, 22, 20, 22, 23
		};

		m_box.vertex_buffer = m_graphics_controller.vertex_buffer_create(vertices, sizeof(vertices));
		m_box.index_buffer = m_graphics_controller.index_buffer_create(indices, sizeof(indices), m_box.index_type);
	}

	set_shadow_map_resolution(2048, 2048);
}

void Renderer::destroy() {
	MY_PROFILE_FUNCTION();

	m_graphics_controller.sync();

	auto free_texture = [&](std::optional<Texture>& texture) {
		if (texture.has_value()) {
			m_image_usage_counts[texture->image]--;
			if (m_image_usage_counts[texture->image] == 0) {
				m_graphics_controller.image_destroy(texture->image);
				m_image_usage_counts.erase(texture->image);
			}

			m_sampler_usage_counts[texture->sampler]--;
			if (m_sampler_usage_counts[texture->sampler] == 0) {
				m_graphics_controller.sampler_destroy(texture->sampler);
				m_image_usage_counts.erase(texture->sampler);
			}
		}
	};

	for (auto& material_pair : m_materials) {
		Material& material = material_pair.second;

		free_texture(material.albedo);
		free_texture(material.ao_rough_met);
		free_texture(material.normal);
		free_texture(material.emissive);
	}
	m_materials.clear();

	for (auto& skybox : m_skyboxes) {
		m_graphics_controller.image_destroy(skybox.second.image);
		m_graphics_controller.uniform_set_destroy(skybox.second.uniform_set_1);
	}
	m_skyboxes.clear();

	m_image_usage_counts.clear();
	m_sampler_usage_counts.clear();

	for (auto& vertex_buffer : m_vertex_buffers)
		m_graphics_controller.buffer_destroy(vertex_buffer.second);
	m_vertex_buffers.clear();

	for (auto& index_buffer : m_index_buffers)
		m_graphics_controller.buffer_destroy(index_buffer.second);
	m_index_buffers.clear();

	m_graphics_controller.destroy();
}

void Renderer::set_resolution(uint32_t width, uint32_t height) {
	// Albedo
	m_deferred.albedo_info.extent.width = width;
	m_deferred.albedo_info.extent.height = height;
	m_deferred.albedo = m_graphics_controller.image_create(m_deferred.albedo_info);

	// Ao-rough-met
	m_deferred.ao_rough_met_info.extent.width = width;
	m_deferred.ao_rough_met_info.extent.height = height;
	m_deferred.ao_rough_met = m_graphics_controller.image_create(m_deferred.ao_rough_met_info);

	// Normals
	m_deferred.normals_info.extent.width = width;
	m_deferred.normals_info.extent.height = height;
	m_deferred.normals = m_graphics_controller.image_create(m_deferred.normals_info);

	m_deferred.emissive_info.extent.width = width;
	m_deferred.emissive_info.extent.height = height;
	m_deferred.emissive = m_graphics_controller.image_create(m_deferred.emissive_info);

	// Depth/stencil
	m_deferred.depth_stencil_info.extent.width = width;
	m_deferred.depth_stencil_info.extent.height = height;
	m_deferred.depth_stencil = m_graphics_controller.image_create(m_deferred.depth_stencil_info);

	// G framebuffer
	std::array<ImageId, 5> g_fb_ids = { m_deferred.albedo, m_deferred.ao_rough_met, m_deferred.normals, m_deferred.emissive, m_deferred.depth_stencil };
	m_deferred.g_framebuffer = m_graphics_controller.framebuffer_create(m_deferred.g_pass, g_fb_ids.data(), (uint32_t)g_fb_ids.size());

	// Composition
	m_deferred.composition_info.extent.width = width;
	m_deferred.composition_info.extent.height = height;
	m_deferred.composition = m_graphics_controller.image_create(m_deferred.composition_info);

	RenderId albedo_ids[2] = { m_deferred.albedo, m_light_pipeline.sampler };
	RenderId ao_rough_met_ids[2] = { m_deferred.ao_rough_met, m_light_pipeline.sampler };
	RenderId normal_ids[2] = { m_deferred.normals, m_light_pipeline.sampler };
	RenderId emissive_ids[2] = { m_deferred.emissive, m_light_pipeline.sampler };
	RenderId depth_ids[2] = { m_deferred.depth_stencil, m_light_pipeline.sampler };

	std::array<UniformInfo, 5> light_set_0_bindings{};
	light_set_0_bindings[0].type = UniformType::CombinedImageSampler;
	light_set_0_bindings[0].subresource_range = { ImageAspectColor };
	light_set_0_bindings[0].binding = 0;
	light_set_0_bindings[0].ids = albedo_ids;
	light_set_0_bindings[0].id_count = 2;
	light_set_0_bindings[1].type = UniformType::CombinedImageSampler;
	light_set_0_bindings[1].subresource_range = { ImageAspectColor };
	light_set_0_bindings[1].binding = 1;
	light_set_0_bindings[1].ids = ao_rough_met_ids;
	light_set_0_bindings[1].id_count = 2;
	light_set_0_bindings[2].type = UniformType::CombinedImageSampler;
	light_set_0_bindings[2].subresource_range = { ImageAspectColor };
	light_set_0_bindings[2].binding = 2;
	light_set_0_bindings[2].ids = normal_ids;
	light_set_0_bindings[2].id_count = 2;
	light_set_0_bindings[3].type = UniformType::CombinedImageSampler;
	light_set_0_bindings[3].subresource_range = { ImageAspectColor };
	light_set_0_bindings[3].binding = 3;
	light_set_0_bindings[3].ids = emissive_ids;
	light_set_0_bindings[3].id_count = 2;
	light_set_0_bindings[4].type = UniformType::CombinedImageSampler;
	light_set_0_bindings[4].subresource_range = { ImageAspectDepth };
	light_set_0_bindings[4].binding = 4;
	light_set_0_bindings[4].ids = depth_ids;
	light_set_0_bindings[4].id_count = 2;

	m_light_pipeline.uniform_set_0 = m_graphics_controller.uniform_set_create(m_light_pipeline.shader, 0, light_set_0_bindings.data(), (uint32_t)light_set_0_bindings.size());

	// Composition
	std::array<ImageId, 2> composition_fb_ids = { m_deferred.composition, m_deferred.depth_stencil };
	m_deferred.composition_framebuffer = m_graphics_controller.framebuffer_create(m_deferred.composition_pass, composition_fb_ids.data(), (uint32_t)composition_fb_ids.size());

	SamplerId sampler;
	if (m_deferred.composition_info.extent.width == m_graphics_controller.screen_resolution().width &&
		m_deferred.composition_info.extent.height == m_graphics_controller.screen_resolution().height)
		sampler = m_present_pipeline.same_res_sampler;
	else
		sampler = m_present_pipeline.diff_res_sampler;

	RenderId present_set_0_bindind_0[2] = { m_deferred.composition, sampler };

	UniformInfo present_uniform_set_0{
		.type = UniformType::CombinedImageSampler,
		.subresource_range = { ImageAspectColor },
		.binding = 0,
		.ids = present_set_0_bindind_0,
		.id_count = 2
	};

	m_present_pipeline.uniform_set_0 = m_graphics_controller.uniform_set_create(m_present_pipeline.shader, 0, &present_uniform_set_0, 1);
}

void Renderer::set_shadow_map_resolution(uint32_t width, uint32_t height) {
	//m_deferred.shadow_map_info.width = width;
	//m_deferred.shadow_map_info.height = height;
	//
	//m_deferred.shadow_map = m_graphics_controller.image_create(nullptr, m_deferred.shadow_map_info);

	//ImageId shadow_map_framebuffer_ids[1] = { m_deferred.shadow_map };
	//m_deferred.shadow_map_framebuffer = m_graphics_controller.framebuffer_create(m_deferred.shadow_map_pass, shadow_map_framebuffer_ids, 1);
}

void Renderer::set_post_effect_constants(float exposure, float gamma) {
	m_scene_info.data.exposure = exposure;
	m_scene_info.data.gamma = gamma;
}

void Renderer::begin_frame(const Camera& camera, Light dir_light, Light* lights, uint32_t light_count) {
	MY_PROFILE_FUNCTION();

	m_scene_info.data.camera = camera;
	m_scene_info.data.light_info.light_dir = dir_light.dir;
	m_scene_info.data.light_info.light_color = dir_light.color;

	// Update camera
	{
		m_scene_info.data.light_info.camera_pos = camera.eye;
		m_graphics_controller.buffer_update(m_scene_info.gpu.view_pos, &camera.eye);

		glm::mat4 view = camera.view_matrix();
		glm::mat4 proj = camera.proj_matrix();
		glm::mat4 proj_view = proj * view;
		glm::mat4 view_no_translation = glm::mat4(glm::mat3(view));
		glm::mat4 skybox_view_proj = proj * view_no_translation;

		m_graphics_controller.buffer_update(m_scene_info.gpu.projview_matrix, &proj_view);
		m_graphics_controller.buffer_update(m_scene_info.gpu.projview_matrix_no_translation, &skybox_view_proj);
	}

	// setup dir light
	{
		glm::mat4 view = glm::lookAt(dir_light.pos, dir_light.pos + dir_light.dir, glm::vec3(0.0f, 1.0f, 0.0f));
		float x = 3.0f;
		float y = 3.0f;
		glm::mat4 proj = glm::ortho(-x, x, -y, y, 0.1f, 10.0f);

		glm::mat4 proj_view = proj * view;

		//m_graphics_controller.buffer_update(m_scene_info.dir_light_projview_matrix, &proj_view);
	}

	// Uptade blend uniform buffer
	m_graphics_controller.buffer_update(m_blend_pipeline.uniform_buffer, &m_scene_info.data.light_info);

	// Save lights
	{
		for (uint32_t i = 0; i < light_count; i++)
			m_lights.push_back(lights[i]);
	}

	m_draw_list.clear();
}

void Renderer::end_frame(uint32_t width, uint32_t height) {
	MY_PROFILE_FUNCTION();

	{
		MY_PROFILE_SCOPE("Render list sorting");

		std::sort(m_draw_list.opaque_primitives.begin(), m_draw_list.opaque_primitives.end(), [](const auto& primitive1, const auto& primitive2) {
			return primitive1.material < primitive2.material;
		});

		std::sort(m_draw_list.blend_primitives.begin(), m_draw_list.blend_primitives.end(), [](const auto& primitive1, const auto& primitive2) {
			return primitive1.material < primitive2.material;
		});
	}

	uint64_t timestamps[2] = { 0 };
	bool timestamps_are_available = m_graphics_controller.timestamp_query_get_results(timestamps, 2);
	if (timestamps_are_available)
		std::cout << "GPU time: " << (float)(timestamps[1] - timestamps[0]) / 1'000'000 << "ms\n";

	m_graphics_controller.timestamp_query_begin();
	m_graphics_controller.timestamp_query_write_timestamp();

	uint32_t stencil_reference = 0x28;
	{
		MY_PROFILE_SCOPE("G pass recording");

		// G pass
		std::array<ClearValue, 5> g_buffer_clear_values{};
		g_buffer_clear_values[0].color = { 0.8f, 0.3f, 0.4f, 1.0f }; // albedo
		g_buffer_clear_values[1].color = { 0.0f, 0.0f, 0.0f, 0.0f }; // ao-rough-met
		g_buffer_clear_values[2].color = { 0.0f, 0.0f, 0.0f, 0.0f }; // normals
		g_buffer_clear_values[3].color = { 0.0f, 0.0f, 0.0f, 0.0f };
		g_buffer_clear_values[4].depth_stencil = { 1.0f, 0 }; // depth-stencil

		m_graphics_controller.draw_begin(m_deferred.g_framebuffer, g_buffer_clear_values.data(), (uint32_t)g_buffer_clear_values.size());
		m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)m_deferred.albedo_info.extent.width, (float)m_deferred.albedo_info.extent.height, 0.0f, 1.0f);
		m_graphics_controller.draw_set_scissor(0, 0, m_deferred.albedo_info.extent.width, m_deferred.albedo_info.extent.height);

		m_graphics_controller.draw_bind_pipeline(m_g_pipeline.pipeline);
		m_graphics_controller.draw_bind_uniform_sets(m_g_pipeline.pipeline, 0, &m_g_pipeline.uniform_set_0, 1);
		m_graphics_controller.draw_set_stencil_reference(StencilFaces::FrontAndBack, stencil_reference);

		MaterialId prev_material = -1;
		BufferId prev_vertex_buffer = -1;
		BufferId prev_index_buffer = -1;
		for (const Primitive& primitive : m_draw_list.opaque_primitives) {
			// If material changed, bind new material
			if (primitive.material != prev_material) {
				const Material& material = m_materials[primitive.material];

				m_graphics_controller.draw_push_constants(m_g_pipeline.shader, ShaderStageFragment, sizeof(glm::mat4), sizeof(MaterialInfo), &material.info);
				m_graphics_controller.draw_bind_uniform_sets(m_g_pipeline.pipeline, 1, &material.uniform_set, 1);
			}
			// If vertex buffer changed, bind new vertex buffer
			if (primitive.vertex_buffer != prev_vertex_buffer)
				m_graphics_controller.draw_bind_vertex_buffer(m_vertex_buffers[primitive.vertex_buffer]);
			// If index buffer changed, bind new index buffer
			if (primitive.index_buffer != prev_index_buffer)
				m_graphics_controller.draw_bind_index_buffer(m_index_buffers[primitive.index_buffer], IndexType::Uint32);

			m_graphics_controller.draw_push_constants(m_g_pipeline.shader, ShaderStageVertex, 0, sizeof(glm::mat4), &primitive.model);

			if (primitive.index_buffer != -1 && primitive.index_count != 0) {
				m_graphics_controller.draw_draw_indexed((uint32_t)primitive.index_count, (uint32_t)primitive.first_index);
				prev_index_buffer = primitive.index_buffer;
			}

			prev_material = primitive.material;
			prev_vertex_buffer = primitive.vertex_buffer;
		}

		m_graphics_controller.draw_end();
	}

	// Composision pass
	std::array<ClearValue, 1> composition_clear_values{};
	composition_clear_values[0].color = { 0.0f, 1.0f, 1.0f, 1.0f };

	m_graphics_controller.draw_begin(m_deferred.composition_framebuffer, composition_clear_values.data(), (uint32_t)composition_clear_values.size());
	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)m_deferred.composition_info.extent.width, (float)m_deferred.composition_info.extent.height, 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, m_deferred.composition_info.extent.width, m_deferred.composition_info.extent.height);

	{
		MY_PROFILE_SCOPE("Lightning recording");

		// Lightning
		glm::mat4 view = m_scene_info.data.camera.view_matrix();
		glm::mat4 proj = m_scene_info.data.camera.proj_matrix();
		glm::mat4 view_inv = glm::inverse(view);
		glm::mat4 proj_inv = glm::inverse(proj);
		glm::mat4 view_proj_inv = view_inv * proj_inv;

		uint8_t push_constants_data[sizeof(glm::mat4) + sizeof(LightInfo)];
		memcpy(push_constants_data, &view_proj_inv, sizeof(glm::mat4));
		memcpy(push_constants_data + sizeof(glm::mat4), &m_scene_info.data.light_info, sizeof(LightInfo));

		m_graphics_controller.draw_bind_pipeline(m_light_pipeline.pipeline);
		m_graphics_controller.draw_bind_vertex_buffer(m_square.vertex_buffer);
		m_graphics_controller.draw_bind_index_buffer(m_square.index_buffer, m_square.index_type);
		m_graphics_controller.draw_push_constants(m_light_pipeline.shader, ShaderStageFragment, 0, sizeof(push_constants_data), push_constants_data);
		m_graphics_controller.draw_bind_uniform_sets(m_light_pipeline.pipeline, 0, &m_light_pipeline.uniform_set_0, 1);
		m_graphics_controller.draw_set_stencil_reference(StencilFaces::FrontAndBack, stencil_reference);
		m_graphics_controller.draw_draw_indexed(m_square.index_count, 0);
	}

	// Draw skybox
	if (m_draw_list.skybox.has_value()) {
		const Skybox& skybox = m_skyboxes[m_draw_list.skybox.value()];

		std::array<UniformSetId, 2> uniform_sets = { m_skybox_pipeline.uniform_set_0, skybox.uniform_set_1 };

		m_graphics_controller.draw_bind_pipeline(m_skybox_pipeline.pipeline);
		m_graphics_controller.draw_bind_vertex_buffer(m_box.vertex_buffer);
		m_graphics_controller.draw_bind_index_buffer(m_box.index_buffer, m_box.index_type);
		m_graphics_controller.draw_bind_uniform_sets(m_skybox_pipeline.pipeline, 0, uniform_sets.data(), (uint32_t)uniform_sets.size());
		m_graphics_controller.draw_draw_indexed(m_box.index_count, 0);
	}

	// Draw coordinate system
	//m_graphics_controller.draw_bind_pipeline(m_coord_system_pipeline.pipeline);
	//m_graphics_controller.draw_bind_vertex_buffer(m_coord_system_pipeline.vertex_buffer);
	//m_graphics_controller.draw_bind_uniform_sets(m_coord_system_pipeline.shader, 0, &m_coord_system_pipeline.uniform_set_0, 1);
	//m_graphics_controller.draw_set_line_width(3.0f);
	//m_graphics_controller.draw_draw(6, 0);

	{
		MY_PROFILE_SCOPE("Transparent pass recording");

		// Blend primitives
		m_graphics_controller.draw_bind_pipeline(m_blend_pipeline.pipeline);
		m_graphics_controller.draw_bind_uniform_sets(m_blend_pipeline.pipeline, 0, &m_blend_pipeline.uniform_set_0, 1);

		MaterialId prev_material = -1;
		BufferId prev_vertex_buffer = -1;
		BufferId prev_index_buffer = -1;
		for (const Primitive& primitive : m_draw_list.blend_primitives) {
			// If material changed, bind new material
			if (primitive.material != prev_material) {
				const Material& material = m_materials[primitive.material];

				m_graphics_controller.draw_push_constants(m_blend_pipeline.shader, ShaderStageFragment, sizeof(glm::mat4), sizeof(MaterialInfo), &material.info);
				m_graphics_controller.draw_bind_uniform_sets(m_blend_pipeline.pipeline, 1, &material.uniform_set, 1);
			}
			// If vertex buffer changed, bind new vertex buffer
			if (primitive.vertex_buffer != prev_vertex_buffer)
				m_graphics_controller.draw_bind_vertex_buffer(m_vertex_buffers[primitive.vertex_buffer]);
			// If index buffer changed, bind new index buffer
			if (primitive.index_buffer != prev_index_buffer)
				m_graphics_controller.draw_bind_index_buffer(m_index_buffers[primitive.index_buffer], IndexType::Uint32);

			m_graphics_controller.draw_push_constants(m_blend_pipeline.shader, ShaderStageVertex, 0, sizeof(glm::mat4), &primitive.model);

			if (primitive.index_buffer != -1 && primitive.index_count != 0) {
				m_graphics_controller.draw_draw_indexed((uint32_t)primitive.index_count, (uint32_t)primitive.first_index);
				prev_index_buffer = primitive.index_buffer;
			}

			prev_material = primitive.material;
			prev_vertex_buffer = primitive.vertex_buffer;
		}
	}

	m_graphics_controller.draw_end();

	// Present to screen pass
	glm::vec4 clear_color{ 1.0f, 0.0f, 1.0f, 1.0f };
	m_graphics_controller.draw_begin_for_screen(clear_color);

	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, width, height);

	float constants[2] = { m_scene_info.data.exposure, m_scene_info.data.gamma };

	m_graphics_controller.draw_bind_pipeline(m_present_pipeline.pipeline);
	m_graphics_controller.draw_bind_vertex_buffer(m_square.vertex_buffer);
	m_graphics_controller.draw_bind_index_buffer(m_square.index_buffer, m_square.index_type);
	m_graphics_controller.draw_bind_uniform_sets(m_present_pipeline.pipeline, 0, &m_present_pipeline.uniform_set_0, 1);
	m_graphics_controller.draw_push_constants(m_present_pipeline.shader, ShaderStageFragment, 0, sizeof(constants), constants);
	m_graphics_controller.draw_draw_indexed(m_square.index_count, 0);

	m_graphics_controller.draw_end_for_screen();

	m_graphics_controller.timestamp_query_write_timestamp();
	m_graphics_controller.timestamp_query_end();

	m_graphics_controller.end_frame();
}

void Renderer::draw_primitive(const glm::mat4& model, size_t vertex_buffer, size_t index_buffer, size_t first_index, size_t index_count, size_t vertex_count, MaterialId material) {
	Primitive primitive{
		.model = model,
		.vertex_buffer = vertex_buffer,
		.index_buffer = index_buffer,
		.first_index = first_index,
		.index_count = index_count,
		.vertex_count = vertex_count,
		.material = material
	};

	if (m_materials[material].alpha_mode == AlphaMode::Blend)
		m_draw_list.blend_primitives.push_back(primitive);
	else
		m_draw_list.opaque_primitives.push_back(primitive);
}

void Renderer::draw_skybox(SkyboxId skybox_id) {
	m_draw_list.skybox = skybox_id;
}

void Renderer::materials_create(ImageSpecs* images, uint32_t image_count, SamplerSpecs* samplers, uint32_t sampler_count, TextureSpecs* textures, uint32_t texture_count, MaterialSpecs* materials, uint32_t material_count, MaterialId* material_ids) {
	MY_PROFILE_FUNCTION();

	std::vector<ImageId> image_ids;
	image_ids.reserve(image_count);

	for (uint32_t i = 0; i < image_count; i++) {
		Extent3D extent{ images[i].width, images[i].height, 1 };

		ImageInfo info{
			.usage = ImageUsageTransferDst | ImageUsageColorSampled,
			.format = Format::RGBA8_SRGB,
			.extent = extent
		};

		ImageSubresourceLayers subresource{
			.aspect = ImageAspectColor,
			.mip_level = 0,
			.base_array_layer = 0,
			.layer_count = 1
		};

		ImageId id = m_graphics_controller.image_create(info);
		m_graphics_controller.image_update(id, subresource, { 0, 0, 0 }, extent, { .format = images[i].data_format, .data = images[i].data });
		
		m_image_usage_counts[id] = 0;

		image_ids.push_back(id);
	}

	std::vector<SamplerId> sampler_ids;
	sampler_ids.reserve(sampler_count);

	for (uint32_t i = 0; i < sampler_count; i++) {
		SamplerInfo info{
			.mag_filter = mag_filter_to_filter(samplers[i].mag_filter),
			.min_filter = min_filter_to_filter(samplers[i].min_filter),
			.mip_map_mode = min_filter_to_mip_map_mode(samplers[i].min_filter),
			.address_mode_u = wrap_to_sampler_address_mode(samplers[i].wrap_u),
			.address_mode_v = wrap_to_sampler_address_mode(samplers[i].wrap_v),
			.anisotropy_enable = true,
			.max_anisotropy = 16.0f
		};

		SamplerId id = m_graphics_controller.sampler_create(info);
		m_sampler_usage_counts[id] = 0;

		sampler_ids.push_back(id);
	}

	for (uint32_t i = 0; i < material_count; i++) {
		Material material{
			.info = materials[i].info,
			.alpha_mode = materials[i].alpha_mode
		};

		Texture albedo_map_texture = { m_defaults.empty_texture.image, m_defaults.empty_texture.sampler };
		Texture ao_rough_met_map_texture = { m_defaults.empty_texture.image, m_defaults.empty_texture.sampler };
		Texture normal_map_texture = { m_defaults.empty_texture.image, m_defaults.empty_texture.sampler };
		Texture emissive_map_texture = { m_defaults.empty_texture.image, m_defaults.empty_texture.sampler };

		std::array<UniformInfo, 4> uniforms;
		uniforms[0].ids = (RenderId*)&albedo_map_texture;
		uniforms[1].ids = (RenderId*)&ao_rough_met_map_texture;
		uniforms[2].ids = (RenderId*)&normal_map_texture;
		uniforms[3].ids = (RenderId*)&emissive_map_texture;

		for (uint32_t j = 0; j < 4; j++) {
			uniforms[j].binding = j;
			uniforms[j].subresource_range = { ImageAspectColor };
			uniforms[j].type = UniformType::CombinedImageSampler;
			uniforms[j].id_count = 2;
		}

		if (materials[i].albedo_id.has_value()) {
			uint32_t texture_id = materials[i].albedo_id.value();
			const TextureSpecs& tex_specs = textures[texture_id];

			ImageId image_id = image_ids[tex_specs.image_id];
			SamplerId sampler_id = sampler_ids[tex_specs.sampler_id];

			albedo_map_texture.image = image_id;
			albedo_map_texture.sampler = sampler_id;

			m_image_usage_counts[image_id]++;
			m_sampler_usage_counts[sampler_id]++;

			material.albedo = albedo_map_texture;
		};
		if (materials[i].ao_rough_met_id.has_value()) {
			uint32_t texture_id = materials[i].ao_rough_met_id.value();
			const TextureSpecs& tex_specs = textures[texture_id];

			ImageId image_id = image_ids[tex_specs.image_id];
			SamplerId sampler_id = sampler_ids[tex_specs.sampler_id];

			ao_rough_met_map_texture.image = image_id;
			ao_rough_met_map_texture.sampler = sampler_id;

			m_image_usage_counts[image_id]++;
			m_sampler_usage_counts[sampler_id]++;

			material.ao_rough_met = ao_rough_met_map_texture;
		}
		if (materials[i].normals_id.has_value()) {
			uint32_t texture_id = materials[i].normals_id.value();
			const TextureSpecs& tex_specs = textures[texture_id];

			ImageId image_id = image_ids[tex_specs.image_id];
			SamplerId sampler_id = sampler_ids[tex_specs.sampler_id];

			normal_map_texture.image = image_id;
			normal_map_texture.sampler = sampler_id;

			m_image_usage_counts[image_id]++;
			m_sampler_usage_counts[sampler_id]++;

			material.normal = normal_map_texture;
		}
		if (materials[i].emissive_id.has_value()) {
			uint32_t texture_id = materials[i].emissive_id.value();
			const TextureSpecs& tex_specs = textures[texture_id];

			ImageId image_id = image_ids[tex_specs.image_id];
			SamplerId sampler_id = sampler_ids[tex_specs.sampler_id];

			emissive_map_texture.image = image_id;
			emissive_map_texture.sampler = sampler_id;

			m_image_usage_counts[image_id]++;
			m_sampler_usage_counts[sampler_id]++;

			material.emissive = emissive_map_texture;
		}

		ShaderId shader_id = materials[i].alpha_mode == AlphaMode::Blend ? m_blend_pipeline.shader : m_g_pipeline.shader;

		material.uniform_set = m_graphics_controller.uniform_set_create(shader_id, 1, uniforms.data(), (uint32_t)uniforms.size());

		m_materials[m_render_id] = std::move(material);
		material_ids[i] = m_render_id;

		m_render_id++;
	}
}

void Renderer::materials_destroy(MaterialId* material_ids, size_t count) {
	for (size_t i = 0; i < count; i++) {
		MaterialId material_id = material_ids[i];

		material_destroy(material_id);

		m_materials.erase(material_id);
	}
}

SkyboxId Renderer::skybox_create(uint32_t cubemap_resolution, const ImageSpecs& texture, SkyboxType type) {
	Extent3D cubemap_extent{ cubemap_resolution, cubemap_resolution, 1 };
	
	ImageInfo skybox_texture_info = {
		.usage = ImageUsageColorSampled | ImageUsageTransferDst,
		.view_type = ImageViewType::Cube,
		.format = texture.desired_format,
		.extent = cubemap_extent,
		.array_layers = 6
	};

	ImageId id = m_graphics_controller.image_create(skybox_texture_info);
	m_image_usage_counts[id]++;
	
	if (type == SkyboxType::Cubemap) {
		ImageSubresourceLayers subresource{
			.aspect = ImageAspectColor,
			.mip_level = 0,
			.base_array_layer = 0,
			.layer_count = 6
		};

		m_graphics_controller.image_update(id, subresource, { 0, 0, 0 }, cubemap_extent, { texture.data_format, texture.data });
	} else if (type == SkyboxType::Equirectangular) {
		Extent3D equirect_image_extent{ texture.width, texture.height, 1 };
		
		ImageInfo equirect_image_info{
			.usage = ImageUsageColorAttachment | ImageUsageColorSampled | ImageUsageTransferSrc | ImageUsageTransferDst,
			.format = texture.data_format,
			.extent = equirect_image_extent
		};
		
		ImageId equirect_image = m_graphics_controller.image_create(equirect_image_info);
		
		ImageSubresourceLayers equirect_image_subres{
			.aspect = ImageAspectColor,
			.mip_level = 0,
			.base_array_layer = 0,
			.layer_count = 1
		};

		ImageDataInfo image_data_info{
			.format = texture.data_format,
			.data = texture.data
		};

		m_graphics_controller.image_update(equirect_image, equirect_image_subres, { 0, 0, 0 }, equirect_image_extent, image_data_info);

		ImageInfo equirectangular_image_info{
			.usage = ImageUsageColorAttachment | ImageUsageTransferSrc,
			.format = texture.desired_format,
			.extent = cubemap_extent,
			.array_layers = 1
		};

		ImageId cubemap_plane = m_graphics_controller.image_create(equirectangular_image_info);

		ImageCopy image_copy{
			.src_subresource = {.aspect = ImageAspectColor, .mip_level = 0, .base_array_layer = 0, .layer_count = 1 },
			.src_offset = { 0, 0, 0 },
			.dst_subresource = {.aspect = ImageAspectColor, .mip_level = 0, .layer_count = 1 },
			.dst_offset = { 0, 0, 0 },
			.extent = cubemap_extent
		};

		FramebufferId skybox_framebuffer = m_graphics_controller.framebuffer_create(m_gen_cubemap_pipeline.render_pass, &cubemap_plane, 1);

		glm::mat4 views[6] =
		{
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};

		glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

		RenderId ids[2] = { equirect_image, m_gen_cubemap_pipeline.sampler };

		UniformInfo uniform_info{
			.type = UniformType::CombinedImageSampler,
			.subresource_range = { ImageAspectColor },
			.binding = 0,
			.ids = ids,
			.id_count = 2
		};

		UniformSetId uniform_set = m_graphics_controller.uniform_set_create(m_gen_cubemap_pipeline.shader, 0, &uniform_info, 1);

		for (int i = 0; i < 6; i++) {
			ClearValue clear_value{ .color = { 0.0f, 0.0f, 0.0f, 0.0f } };
			m_graphics_controller.draw_begin(skybox_framebuffer, &clear_value, 1);
			m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)cubemap_resolution, (float)cubemap_resolution, 0.0f, 1.0f);
			m_graphics_controller.draw_set_scissor(0, 0, cubemap_resolution, cubemap_resolution);

			char buffers[128];
			memcpy(buffers, &proj, sizeof(glm::mat4));
			memcpy(buffers + sizeof(glm::mat4), &views[i], sizeof(glm::mat4));

			m_graphics_controller.draw_bind_vertex_buffer(m_box.vertex_buffer);
			m_graphics_controller.draw_bind_index_buffer(m_box.index_buffer, m_box.index_type);
			m_graphics_controller.draw_bind_pipeline(m_gen_cubemap_pipeline.pipeline);
			m_graphics_controller.draw_push_constants(m_gen_cubemap_pipeline.shader, ShaderStageVertex, 0, 2 * sizeof(glm::mat4), buffers);
			m_graphics_controller.draw_bind_uniform_sets(m_gen_cubemap_pipeline.pipeline, 0, &uniform_set, 1);
			m_graphics_controller.draw_draw_indexed(m_box.index_count, 0);

			m_graphics_controller.draw_end();

			image_copy.dst_subresource.base_array_layer = i;

			m_graphics_controller.image_copy(cubemap_plane, id, image_copy);
		}

		m_graphics_controller.framebuffer_destroy(skybox_framebuffer);
	}
	
	RenderId texture_ids[2] = { id, m_skybox_pipeline.sampler };

	UniformInfo skybox_texture_uniform{
		.type = UniformType::CombinedImageSampler,
		.subresource_range = { .aspect = ImageAspectColor, .layer_count = 6 },
		.binding = 0,
		.ids = texture_ids,
		.id_count = 2
	};

	Skybox skybox{ id, m_graphics_controller.uniform_set_create(m_skybox_pipeline.shader, 1, &skybox_texture_uniform, 1) };

	m_skyboxes[m_render_id] = std::move(skybox);
	return m_render_id++;
}

void Renderer::skybox_destroy(SkyboxId skybox_id) {
	Skybox& skybox = m_skyboxes.at(skybox_id);

	clear_image(skybox.image);
	m_graphics_controller.uniform_set_destroy(skybox.uniform_set_1);

	m_skyboxes.erase(skybox_id);
}

VertexBufferId Renderer::vertex_buffer_create(const Vertex* data, size_t count) {
	MY_PROFILE_FUNCTION();

	BufferId buffer_id = m_graphics_controller.vertex_buffer_create(data, count * sizeof(Vertex));

	m_vertex_buffers[m_render_id] = buffer_id;

	return m_render_id++;
}

IndexBufferId Renderer::index_buffer_create(const uint32_t* data, size_t count) {
	MY_PROFILE_FUNCTION();

	BufferId buffer_id = m_graphics_controller.index_buffer_create(data, count * 4, IndexType::Uint32);

	m_index_buffers[m_render_id] = buffer_id;

	return m_render_id++;
}

void Renderer::material_destroy(MaterialId material_id) {
	Material& material = m_materials.at(material_id);

	if (material.albedo.has_value()) {
		clear_image(material.albedo->image);
		clear_sampler(material.albedo->sampler);
	} if (material.ao_rough_met.has_value()) {
		clear_image(material.ao_rough_met->image);
		clear_sampler(material.ao_rough_met->sampler);
	} if (material.normal.has_value()) {
		clear_image(material.normal->image);
		clear_sampler(material.normal->sampler);
	} if (material.emissive.has_value()) {
		clear_image(material.emissive->image);
		clear_sampler(material.emissive->sampler);
	}
}

void Renderer::clear_image(ImageId image_id) {
	size_t& image_count = m_image_usage_counts.at(image_id);

	if (image_count == 0)
		return;
	else if (image_count == 1) {
		image_count = 0;
		m_graphics_controller.image_destroy(image_id);
		m_image_usage_counts.erase(image_id);
	} else
		image_count--;
}

void Renderer::clear_sampler(SamplerId sampler_id) {
	size_t& sampler_count = m_sampler_usage_counts.at(sampler_id);

	if (sampler_count == 0)
		return;
	if (sampler_count == 1) {
		sampler_count = 0;
		m_graphics_controller.sampler_destroy(sampler_id);
		m_sampler_usage_counts.erase(sampler_id);
	} else
		sampler_count--;
}