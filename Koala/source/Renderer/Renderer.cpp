#include "Renderer.h"

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
	m_graphics_controller.create(context);

	// Create empty texture
	{
		int8_t zeroes[4] = { 0, 0, 0, 0 };
		ImageInfo empty_texture_info{
			.usage = ImageUsageTransferDst | ImageUsageColorSampled,
			.view_type = ImageViewType::TwoD,
			.format = Format::BGRA8_UNorm,
			.width = 1,
			.height = 1,
			.depth = 1,
			.layer_count = 1
		};
		m_defaults.empty_texture.image = m_graphics_controller.image_create(zeroes, empty_texture_info);

		SamplerInfo empty_texture_sampler_info{
			.mag_filter = Filter::Nearest,
			.min_filter = Filter::Nearest,
			.mip_map_mode = MipMapMode::Nearest
		};
		m_defaults.empty_texture.sampler = m_graphics_controller.sampler_create(empty_texture_sampler_info);
	}

	// Setup scene resources
	m_scene_info.view_pos = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::vec3));
	m_scene_info.proj_inv_matrix = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));
	m_scene_info.view_inv_matrix = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));
	m_scene_info.projview_matrix = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));
	m_scene_info.projview_matrix_no_translation = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(glm::mat4));

	// Create deferred render targets
	{
		m_deferred.albedo_info.usage = ImageUsageColorAttachment | ImageUsageColorSampled;
		m_deferred.albedo_info.view_type = ImageViewType::TwoD;
		m_deferred.albedo_info.format = Format::BGRA8_UNorm;
		m_deferred.albedo_info.depth = 1;
		m_deferred.albedo_info.layer_count = 1;

		m_deferred.ao_rough_met_info.usage = ImageUsageColorAttachment | ImageUsageColorSampled;
		m_deferred.ao_rough_met_info.view_type = ImageViewType::TwoD;
		m_deferred.ao_rough_met_info.format = Format::BGRA8_UNorm;
		m_deferred.ao_rough_met_info.depth = 1;
		m_deferred.ao_rough_met_info.layer_count = 1;

		m_deferred.normals_info.usage = ImageUsageColorAttachment | ImageUsageColorSampled;
		m_deferred.normals_info.view_type = ImageViewType::TwoD;
		m_deferred.normals_info.format = Format::RGBA8_SNorm;
		m_deferred.normals_info.depth = 1;
		m_deferred.normals_info.layer_count = 1;

		m_deferred.emissive_info.usage = ImageUsageColorAttachment | ImageUsageColorSampled;
		m_deferred.emissive_info.view_type = ImageViewType::TwoD;
		m_deferred.emissive_info.format = Format::RGBA8_UNorm;
		m_deferred.emissive_info.depth = 1;
		m_deferred.emissive_info.layer_count = 1;

		m_deferred.depth_stencil_info.usage = ImageUsageDepthStencilAttachment | ImageUsageDepthSampled;
		m_deferred.depth_stencil_info.view_type = ImageViewType::TwoD;
		m_deferred.depth_stencil_info.format = Format::D24_UNorm_S8_UInt;
		m_deferred.depth_stencil_info.depth = 1;
		m_deferred.depth_stencil_info.layer_count = 1;

		m_deferred.depth_info.usage = ImageUsageColorAttachment | ImageUsageColorSampled;
		m_deferred.depth_info.view_type = ImageViewType::TwoD;
		m_deferred.depth_info.format = Format::R32_SFloat;
		m_deferred.depth_info.depth = 1;
		m_deferred.depth_info.layer_count = 1;

		m_deferred.composition_info.usage = ImageUsageColorAttachment | ImageUsageColorSampled;
		m_deferred.composition_info.view_type = ImageViewType::TwoD;
		m_deferred.composition_info.format = Format::RGBA16_SFloat;
		m_deferred.composition_info.depth = 1;
		m_deferred.composition_info.layer_count = 1;

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
		g_pass_attachments[4].previous_usage = ImageUsageDepthSampled;
		g_pass_attachments[4].current_usage = ImageUsageDepthStencilAttachment;
		g_pass_attachments[4].next_usage = ImageUsageDepthSampled;
		g_pass_attachments[4].format = m_deferred.depth_stencil_info.format;
		g_pass_attachments[4].initial_action = InitialAction::Clear;
		g_pass_attachments[4].final_action = FinalAction::Store;
		g_pass_attachments[4].stencil_initial_action = InitialAction::Clear;
		g_pass_attachments[4].stencil_final_action = FinalAction::Store;

		m_deferred.g_pass = m_graphics_controller.render_pass_create(g_pass_attachments.data(), (uint32_t)g_pass_attachments.size());

		// Depth copy pass
		std::array<RenderPassAttachment, 1> depth_copy_attachments{};
		depth_copy_attachments[0].previous_usage = ImageUsageColorSampled;
		depth_copy_attachments[0].current_usage = ImageUsageColorAttachment;
		depth_copy_attachments[0].next_usage = ImageUsageColorSampled;
		depth_copy_attachments[0].format = m_deferred.depth_info.format;
		depth_copy_attachments[0].initial_action = InitialAction::Clear;
		depth_copy_attachments[0].final_action = FinalAction::Store;

		m_deferred.depth_copy_pass = m_graphics_controller.render_pass_create(depth_copy_attachments.data(), (uint32_t)depth_copy_attachments.size());

		std::array<RenderPassAttachment, 2> composition_attachments{};
		composition_attachments[0].previous_usage = ImageUsageColorSampled;
		composition_attachments[0].current_usage = ImageUsageColorAttachment;
		composition_attachments[0].next_usage = ImageUsageColorSampled;
		composition_attachments[0].format = m_deferred.composition_info.format;
		composition_attachments[0].initial_action = InitialAction::Clear;
		composition_attachments[0].final_action = FinalAction::Store;
		composition_attachments[1].previous_usage = ImageUsageDepthSampled;
		composition_attachments[1].current_usage = ImageUsageDepthStencilAttachment;
		composition_attachments[1].next_usage = ImageUsageDepthStencilAttachment;
		composition_attachments[1].format = m_deferred.depth_stencil_info.format;
		composition_attachments[1].initial_action = InitialAction::Load;
		composition_attachments[1].final_action = FinalAction::Store;

		m_deferred.composition_pass = m_graphics_controller.render_pass_create(composition_attachments.data(), (uint32_t)composition_attachments.size());
	}

	{}

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
		g_pipeline_info.depth_stencil.stencil_test_enable = true;
		g_pipeline_info.depth_stencil.depth_compare_op = CompareOp::Less;
		g_pipeline_info.color_blend.attachment_count = (uint32_t)blend_attachments.size();
		g_pipeline_info.color_blend.attachments = blend_attachments.data();
		g_pipeline_info.render_pass_id = m_deferred.g_pass;

		m_g_pipeline.pipeline = m_graphics_controller.pipeline_create(g_pipeline_info);

		UniformInfo g_pipeline_uniform_set_0;
		g_pipeline_uniform_set_0.type = UniformType::UniformBuffer;
		g_pipeline_uniform_set_0.binding = 0;
		g_pipeline_uniform_set_0.ids = &m_scene_info.projview_matrix;
		g_pipeline_uniform_set_0.id_count = 1;

		m_g_pipeline.uniform_set_0 = m_graphics_controller.uniform_set_create(m_g_pipeline.shader, 0, &g_pipeline_uniform_set_0, 1);
	}

	// Create depth copy pipeline
	{
		auto vert_spv = load_spv("../assets/shaders/present.vert.spv");
		auto frag_spv = load_spv("../assets/shaders/depth_copy.frag.spv");

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

		m_depth_copy_pipeline.shader = m_graphics_controller.shader_create(shader_stages.data(), (uint32_t)shader_stages.size());

		std::array<PipelineDynamicStateFlags, 2> dynamic_states = { DYNAMIC_STATE_VIEWPORT, DYNAMIC_STATE_SCISSOR };

		std::array<ColorBlendAttachmentState, 1> depth_copy_attachments{};
		depth_copy_attachments[0].blend_enable = false;
		depth_copy_attachments[0].color_write_mask = ColorComponentR;

		PipelineInfo depth_copy_pipeline_info{};
		depth_copy_pipeline_info.shader_id = m_depth_copy_pipeline.shader;
		depth_copy_pipeline_info.dynamic_states.dynamic_state_count = (uint32_t)dynamic_states.size();
		depth_copy_pipeline_info.dynamic_states.dynamic_states = dynamic_states.data();
		depth_copy_pipeline_info.depth_stencil.depth_test_enable = false;
		depth_copy_pipeline_info.color_blend.attachment_count = (uint32_t)depth_copy_attachments.size();
		depth_copy_pipeline_info.color_blend.attachments = depth_copy_attachments.data();
		depth_copy_pipeline_info.render_pass_id = m_deferred.depth_copy_pass;

		m_depth_copy_pipeline.pipeline = m_graphics_controller.pipeline_create(depth_copy_pipeline_info);

		SamplerInfo sampler_info{
			.mag_filter = Filter::Nearest,
			.min_filter = Filter::Nearest,
			.mip_map_mode = MipMapMode::Nearest
		};

		m_depth_copy_pipeline.sampler = m_graphics_controller.sampler_create(sampler_info);
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
		blend_pipeline_info.render_pass_id = m_deferred.composition_pass;

		m_blend_pipeline.pipeline = m_graphics_controller.pipeline_create(blend_pipeline_info);

		m_blend_pipeline.uniform_buffer = m_graphics_controller.uniform_buffer_create(nullptr, sizeof(LightInfo));

		std::array<UniformInfo, 2> blend_pipeline_uniform_set_0;
		blend_pipeline_uniform_set_0[0].type = UniformType::UniformBuffer;
		blend_pipeline_uniform_set_0[0].binding = 0;
		blend_pipeline_uniform_set_0[0].ids = &m_scene_info.projview_matrix;
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
		skybox_pipeline_info.depth_stencil.depth_write_enable = true;
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
			.ids = &m_scene_info.projview_matrix_no_translation,
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
			.ids = &m_scene_info.projview_matrix,
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
	m_graphics_controller.destroy();
}

void Renderer::set_resolution(uint32_t width, uint32_t height) {
	// Albedo
	m_deferred.albedo_info.width = width;
	m_deferred.albedo_info.height = height;
	m_deferred.albedo = m_graphics_controller.image_create(nullptr, m_deferred.albedo_info);

	// Ao-rough-met
	m_deferred.ao_rough_met_info.width = width;
	m_deferred.ao_rough_met_info.height = height;
	m_deferred.ao_rough_met = m_graphics_controller.image_create(nullptr, m_deferred.ao_rough_met_info);

	// Normals
	m_deferred.normals_info.width = width;
	m_deferred.normals_info.height = height;
	m_deferred.normals = m_graphics_controller.image_create(nullptr, m_deferred.normals_info);

	m_deferred.emissive_info.width = width;
	m_deferred.emissive_info.height = height;
	m_deferred.emissive = m_graphics_controller.image_create(nullptr, m_deferred.emissive_info);

	// Depth/stencil
	m_deferred.depth_stencil_info.width = width;
	m_deferred.depth_stencil_info.height = height;
	m_deferred.depth_stencil = m_graphics_controller.image_create(nullptr, m_deferred.depth_stencil_info);

	// Depth
	m_deferred.depth_info.width = width;
	m_deferred.depth_info.height = height;
	m_deferred.depth = m_graphics_controller.image_create(nullptr, m_deferred.depth_info);

	RenderId depth_copy_ids[2] = { m_deferred.depth_stencil, m_depth_copy_pipeline.sampler };

	UniformInfo depth_copy_uniform_set_0{
		.type = UniformType::CombinedImageSampler,
		.image_usage = ImageUsageDepthSampled,
		.binding = 0,
		.ids = depth_copy_ids,
		.id_count = 2
	};

	m_depth_copy_pipeline.uniform_set_0 = m_graphics_controller.uniform_set_create(m_depth_copy_pipeline.shader, 0, &depth_copy_uniform_set_0, 1);

	// G framebuffer
	std::array<ImageId, 5> g_fb_ids = { m_deferred.albedo, m_deferred.ao_rough_met, m_deferred.normals, m_deferred.emissive, m_deferred.depth_stencil };
	m_deferred.g_framebuffer = m_graphics_controller.framebuffer_create(m_deferred.g_pass, g_fb_ids.data(), (uint32_t)g_fb_ids.size());

	std::array<ImageId, 1> depth_copy_fb_ids = { m_deferred.depth };
	m_deferred.depth_copy_framebuffer = m_graphics_controller.framebuffer_create(m_deferred.depth_copy_pass, depth_copy_fb_ids.data(), (uint32_t)depth_copy_fb_ids.size());

	// Screen-space shadow map
	//m_deferred.ss_shadows_info.width = width;
	//m_deferred.ss_shadows_info.height = height;
	//m_deferred.ss_shadows = m_graphics_controller.image_create(nullptr, m_deferred.ss_shadows_info);

	// Composition
	m_deferred.composition_info.width = width;
	m_deferred.composition_info.height = height;
	m_deferred.composition = m_graphics_controller.image_create(nullptr, m_deferred.composition_info);

	//std::array<ImageId, 1> ss_shadows_fb_ids = { m_deferred.ss_shadows };
	//m_deferred.ss_shadows_framebuffer = m_graphics_controller.framebuffer_create(m_deferred.ss_shadows_pass, ss_shadows_fb_ids.data(), (uint32_t)ss_shadows_fb_ids.size());

	// Light
	//std::array<ImageId, 2> light_fb_ids = { m_deferred.composition, m_deferred.depth_stencil };
	//m_deferred.light_framebuffer = m_graphics_controller.framebuffer_create(m_deferred.light_pass, light_fb_ids.data(), (uint32_t)light_fb_ids.size());

	RenderId albedo_ids[2] = { m_deferred.albedo, m_light_pipeline.sampler };
	RenderId ao_rough_met_ids[2] = { m_deferred.ao_rough_met, m_light_pipeline.sampler };
	RenderId normal_ids[2] = { m_deferred.normals, m_light_pipeline.sampler };
	RenderId emissive_ids[2] = { m_deferred.emissive, m_light_pipeline.sampler };
	RenderId depth_ids[2] = { m_deferred.depth, m_light_pipeline.sampler };

	std::array<UniformInfo, 7> light_set_0_bindings{};
	light_set_0_bindings[0].type = UniformType::CombinedImageSampler;
	light_set_0_bindings[0].image_usage = ImageUsageColorSampled;
	light_set_0_bindings[0].binding = 0;
	light_set_0_bindings[0].ids = albedo_ids;
	light_set_0_bindings[0].id_count = 2;
	light_set_0_bindings[1].type = UniformType::CombinedImageSampler;
	light_set_0_bindings[1].image_usage = ImageUsageColorSampled;
	light_set_0_bindings[1].binding = 1;
	light_set_0_bindings[1].ids = ao_rough_met_ids;
	light_set_0_bindings[1].id_count = 2;
	light_set_0_bindings[2].type = UniformType::CombinedImageSampler;
	light_set_0_bindings[2].image_usage = ImageUsageColorSampled;
	light_set_0_bindings[2].binding = 2;
	light_set_0_bindings[2].ids = normal_ids;
	light_set_0_bindings[2].id_count = 2;
	light_set_0_bindings[3].type = UniformType::CombinedImageSampler;
	light_set_0_bindings[3].image_usage = ImageUsageColorSampled;
	light_set_0_bindings[3].binding = 3;
	light_set_0_bindings[3].ids = emissive_ids;
	light_set_0_bindings[3].id_count = 2;
	light_set_0_bindings[4].type = UniformType::CombinedImageSampler;
	light_set_0_bindings[4].image_usage = ImageUsageColorSampled;
	light_set_0_bindings[4].binding = 4;
	light_set_0_bindings[4].ids = depth_ids;
	light_set_0_bindings[4].id_count = 2;
	light_set_0_bindings[5].type = UniformType::UniformBuffer;
	light_set_0_bindings[5].binding = 5;
	light_set_0_bindings[5].ids = &m_scene_info.proj_inv_matrix;
	light_set_0_bindings[5].id_count = 1;
	light_set_0_bindings[6].type = UniformType::UniformBuffer;
	light_set_0_bindings[6].binding = 6;
	light_set_0_bindings[6].ids = &m_scene_info.view_inv_matrix;
	light_set_0_bindings[6].id_count = 1;

	m_light_pipeline.uniform_set_0 = m_graphics_controller.uniform_set_create(m_light_pipeline.shader, 0, light_set_0_bindings.data(), (uint32_t)light_set_0_bindings.size());

	// Composition
	std::array<ImageId, 2> composition_fb_ids = { m_deferred.composition, m_deferred.depth_stencil };
	m_deferred.composition_framebuffer = m_graphics_controller.framebuffer_create(m_deferred.composition_pass, composition_fb_ids.data(), (uint32_t)composition_fb_ids.size());

	SamplerId sampler;
	if (m_deferred.composition_info.width == m_graphics_controller.screen_resolution().width &&
		m_deferred.composition_info.height == m_graphics_controller.screen_resolution().height)
		sampler = m_present_pipeline.same_res_sampler;
	else
		sampler = m_present_pipeline.diff_res_sampler;

	RenderId present_set_0_bindind_0[2] = { m_deferred.composition, sampler };

	UniformInfo present_uniform_set_0{
		.type = UniformType::CombinedImageSampler,
		.image_usage = ImageUsageColorSampled,
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

void Renderer::begin_frame(const Camera& camera, Light dir_light, Light* lights, uint32_t light_count) {
	m_scene_info.camera = camera;
	m_scene_info.light_info.light_dir = dir_light.dir;
	m_scene_info.light_info.light_color = dir_light.color;

	// Update camera
	{
		m_scene_info.light_info.camera_pos = camera.eye;
		m_graphics_controller.buffer_update(m_scene_info.view_pos, &camera.eye);

		glm::mat4 view = camera.view_matrix();
		glm::mat4 proj = camera.proj_matrix();
		glm::mat4 view_inv = glm::inverse(view);
		glm::mat4 proj_inv = glm::inverse(proj);
		glm::mat4 proj_view = proj * view;
		glm::mat4 view_no_translation = glm::mat4(glm::mat3(view));
		glm::mat4 skybox_view_proj = proj * view_no_translation;

		m_graphics_controller.buffer_update(m_scene_info.proj_inv_matrix, &proj_inv);
		m_graphics_controller.buffer_update(m_scene_info.view_inv_matrix, &view_inv);
		m_graphics_controller.buffer_update(m_scene_info.projview_matrix, &proj_view);
		m_graphics_controller.buffer_update(m_scene_info.projview_matrix_no_translation, &skybox_view_proj);
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
	m_graphics_controller.buffer_update(m_blend_pipeline.uniform_buffer, &m_scene_info.light_info);

	// Save lights
	{
		for (uint32_t i = 0; i < light_count; i++)
			m_lights.push_back(lights[i]);
	}

	m_draw_list.clear();
}

void Renderer::end_frame(uint32_t width, uint32_t height) {
	std::sort(m_draw_list.opaque_primitives.begin(), m_draw_list.opaque_primitives.end(), [](const auto& primitive1, const auto& primitive2) {
		return primitive1.material < primitive2.material;
		});

	std::sort(m_draw_list.blend_primitives.begin(), m_draw_list.blend_primitives.end(), [](const auto& primitive1, const auto& primitive2) {
		return primitive1.material < primitive2.material;
		});

	// G pass
	std::array<ClearValue, 5> g_buffer_clear_values{};
	g_buffer_clear_values[0].color = { 0.8f, 0.3f, 0.4f, 1.0f }; // albedo
	g_buffer_clear_values[1].color = { 0.0f, 0.0f, 0.0f, 0.0f }; // ao-rough-met
	g_buffer_clear_values[2].color = { 0.0f, 0.0f, 0.0f, 0.0f }; // normals
	g_buffer_clear_values[3].color = { 0.0f, 0.0f, 0.0f, 0.0f };
	g_buffer_clear_values[4].depth_stencil = { 1.0f, 0 }; // depth-stencil

	m_graphics_controller.draw_begin(m_deferred.g_framebuffer, g_buffer_clear_values.data(), (uint32_t)g_buffer_clear_values.size());
	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)m_deferred.albedo_info.width, (float)m_deferred.albedo_info.height, 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, m_deferred.albedo_info.width, m_deferred.albedo_info.height);

	uint32_t stencil_reference = 0x28;
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


	// Depth copy pass
	std::array<ClearValue, 1> depth_copy_clear_values{};
	depth_copy_clear_values[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };

	m_graphics_controller.draw_begin(m_deferred.depth_copy_framebuffer, depth_copy_clear_values.data(), (uint32_t)depth_copy_clear_values.size());
	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)m_deferred.depth_info.width, (float)m_deferred.depth_info.height, 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, m_deferred.depth_info.width, m_deferred.depth_info.height);

	m_graphics_controller.draw_bind_pipeline(m_depth_copy_pipeline.pipeline);
	m_graphics_controller.draw_bind_vertex_buffer(m_square.vertex_buffer);
	m_graphics_controller.draw_bind_index_buffer(m_square.index_buffer, m_square.index_type);
	m_graphics_controller.draw_bind_uniform_sets(m_depth_copy_pipeline.pipeline, 0, &m_depth_copy_pipeline.uniform_set_0, 1);
	m_graphics_controller.draw_draw_indexed(m_square.index_count, 0);

	m_graphics_controller.draw_end();


	// Composision pass
	std::array<ClearValue, 1> composition_clear_values{};
	composition_clear_values[0].color = { 0.0f, 1.0f, 1.0f, 1.0f };

	m_graphics_controller.draw_begin(m_deferred.composition_framebuffer, composition_clear_values.data(), (uint32_t)composition_clear_values.size());
	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)m_deferred.composition_info.width, (float)m_deferred.composition_info.height, 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, m_deferred.composition_info.width, m_deferred.composition_info.height);

	// Lightning
	m_graphics_controller.draw_bind_pipeline(m_light_pipeline.pipeline);
	m_graphics_controller.draw_bind_vertex_buffer(m_square.vertex_buffer);
	m_graphics_controller.draw_bind_index_buffer(m_square.index_buffer, m_square.index_type);
	m_graphics_controller.draw_push_constants(m_light_pipeline.shader, ShaderStageFragment, 0, sizeof(LightInfo), &m_scene_info.light_info);
	m_graphics_controller.draw_bind_uniform_sets(m_light_pipeline.pipeline, 0, &m_light_pipeline.uniform_set_0, 1);
	m_graphics_controller.draw_set_stencil_reference(StencilFaces::FrontAndBack, stencil_reference);
	m_graphics_controller.draw_draw_indexed(m_square.index_count, 0);

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

	// Blend primitives
	m_graphics_controller.draw_bind_pipeline(m_blend_pipeline.pipeline);
	m_graphics_controller.draw_bind_uniform_sets(m_blend_pipeline.pipeline, 0, &m_blend_pipeline.uniform_set_0, 1);
	
	prev_material = -1;
	prev_vertex_buffer = -1;
	prev_index_buffer = -1;
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

	m_graphics_controller.draw_end();

	//// Generate Shadow map
	//ClearValue shadow_map_clear_value{ .depth_stencil = { 1.0f, 0 } };
	//m_graphics_controller.draw_begin(m_shadow_map_target.framebuffer, &shadow_map_clear_value, 1);
	//
	//m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)m_shadow_map_target.image_infos[0].width, (float)m_shadow_map_target.image_infos[0].height, 0.0f, 1.0f);
	//m_graphics_controller.draw_set_scissor(0, 0, m_shadow_map_target.image_infos[0].width, m_shadow_map_target.image_infos[0].height);
	//
	//m_graphics_controller.draw_bind_pipeline(m_shadow_generation.pipeline);
	//m_graphics_controller.draw_bind_uniform_sets(m_shadow_generation.pipeline, 0, &m_shadow_generation.shadow_gen_uniform_set_0, 1);
	//for (MeshId mesh_id : m_draw_list.meshes) {
	//	const Mesh& mesh = m_meshes[mesh_id];
	//
	//	m_graphics_controller.draw_bind_vertex_buffer(mesh.vertex_buffer);
	//	m_graphics_controller.draw_bind_index_buffer(mesh.index_buffer, IndexType::Uint32);
	//	m_graphics_controller.draw_bind_uniform_sets(m_shadow_generation.pipeline, 1, &mesh.shadow_gen_uniform_set_1, 1);
	//	m_graphics_controller.draw_draw_indexed(mesh.index_count);
	//}
	//
	//m_graphics_controller.draw_end();

	//// Draw to offscreen buffer
	//ClearValue offscreen_clear_values[2]{};
	//offscreen_clear_values[0].color = { 0.9f, 0.7f, 0.8f, 1.0f };
	//offscreen_clear_values[1].depth_stencil = { 1.0f, 0 };
	//m_graphics_controller.draw_begin(m_offscreen_render_target.framebuffer, offscreen_clear_values, 2);
	//
	//m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)m_offscreen_render_target.image_infos[0].width, (float)m_offscreen_render_target.image_infos[0].height, 0.0f, 1.0f);
	//m_graphics_controller.draw_set_scissor(0, 0, m_offscreen_render_target.image_infos[0].width, m_offscreen_render_target.image_infos[0].height);

	//// Draw meshes
	//m_graphics_controller.draw_bind_pipeline(m_mesh_common.pipeline);
	//m_graphics_controller.draw_bind_uniform_sets(m_mesh_common.pipeline, 1, &m_mesh_common.draw_uniform_set_1, 1);
	//for (MeshId mesh_id : m_draw_list.meshes) {
	//	const Mesh& mesh = m_meshes[mesh_id];
	//	
	//	m_graphics_controller.draw_bind_vertex_buffer(mesh.vertex_buffer);
	//	m_graphics_controller.draw_bind_index_buffer(mesh.index_buffer, IndexType::Uint32);
	//	m_graphics_controller.draw_bind_uniform_sets(m_mesh_common.pipeline, 0, &mesh.draw_uniform_set_0, 1);
	//	m_graphics_controller.draw_draw_indexed(mesh.index_count);
	//}


	// Present to screen pass
	glm::vec4 clear_color{ 1.0f, 0.0f, 1.0f, 1.0f };
	m_graphics_controller.draw_begin_for_screen(clear_color);

	m_graphics_controller.draw_set_viewport(0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
	m_graphics_controller.draw_set_scissor(0, 0, width, height);

	m_graphics_controller.draw_bind_pipeline(m_present_pipeline.pipeline);
	m_graphics_controller.draw_bind_vertex_buffer(m_square.vertex_buffer);
	m_graphics_controller.draw_bind_index_buffer(m_square.index_buffer, m_square.index_type);
	m_graphics_controller.draw_bind_uniform_sets(m_present_pipeline.pipeline, 0, &m_present_pipeline.uniform_set_0, 1);
	m_graphics_controller.draw_draw_indexed(m_square.index_count, 0);

	m_graphics_controller.draw_end_for_screen();

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

MaterialId Renderer::materials_create(ImageSpecs* images, uint32_t image_count, SamplerSpecs* samplers, uint32_t sampler_count, TextureSpecs* textures, uint32_t texture_count, MaterialSpecs* materials, uint32_t material_count) {
	MaterialId first_material_offset = (uint32_t)m_materials.size();

	std::vector<ImageId> image_ids;
	image_ids.reserve(image_count);

	for (uint32_t i = 0; i < image_count; i++) {
		ImageInfo info{
			.usage = ImageUsageTransferDst | ImageUsageColorSampled,
			.format = Format::RGBA8_SRGB,
			.width = images[i].width,
			.height = images[i].height
		};

		ImageId id = m_graphics_controller.image_create(images[i].data, info);
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
		sampler_ids.push_back(id);
	}

	m_materials.reserve(m_materials.size() + material_count);

	for (uint32_t i = 0; i < material_count; i++) {
		Material material{
			.info = materials[i].info,
			.alpha_mode = materials[i].alpha_mode
		};

		RenderId albedo_map_ids[2] = { m_defaults.empty_texture.image, m_defaults.empty_texture.sampler };
		RenderId ao_rough_met_map_ids[2] = { m_defaults.empty_texture.image, m_defaults.empty_texture.sampler };
		RenderId normal_map_ids[2] = { m_defaults.empty_texture.image, m_defaults.empty_texture.sampler };
		RenderId emissive_map_ids[2] = { m_defaults.empty_texture.image, m_defaults.empty_texture.sampler };

		std::array<UniformInfo, 4> uniforms;
		uniforms[0].ids = albedo_map_ids;
		uniforms[1].ids = ao_rough_met_map_ids;
		uniforms[2].ids = normal_map_ids;
		uniforms[3].ids = emissive_map_ids;

		for (uint32_t j = 0; j < 4; j++) {
			uniforms[j].binding = j;
			uniforms[j].image_usage = ImageUsageColorSampled;
			uniforms[j].type = UniformType::CombinedImageSampler;
			uniforms[j].id_count = 2;
		}

		if (materials[i].albedo_id.has_value()) {
			uint32_t texture_id = materials[i].albedo_id.value();
			const TextureSpecs& tex_specs = textures[texture_id];

			albedo_map_ids[0] = image_ids[tex_specs.image_id];
			albedo_map_ids[1] = sampler_ids[tex_specs.sampler_id];

			material.has_albedo_map = true;
		};
		if (materials[i].ao_rough_met_id.has_value()) {
			uint32_t texture_id = materials[i].ao_rough_met_id.value();
			const TextureSpecs& tex_specs = textures[texture_id];

			ao_rough_met_map_ids[0] = image_ids[tex_specs.image_id];
			ao_rough_met_map_ids[1] = sampler_ids[tex_specs.sampler_id];

			material.has_ao_rough_met_map = true;
		}
		if (materials[i].normals_id.has_value()) {
			uint32_t texture_id = materials[i].normals_id.value();
			const TextureSpecs& tex_specs = textures[texture_id];

			normal_map_ids[0] = image_ids[tex_specs.image_id];
			normal_map_ids[1] = sampler_ids[tex_specs.sampler_id];

			material.has_normal_map = true;
		}
		if (materials[i].emissive_id.has_value()) {
			uint32_t texture_id = materials[i].emissive_id.value();
			const TextureSpecs& tex_specs = textures[texture_id];

			emissive_map_ids[0] = image_ids[tex_specs.image_id];
			emissive_map_ids[1] = sampler_ids[tex_specs.sampler_id];

			material.has_emissive_map = true;
		}

		ShaderId shader_id = materials[i].alpha_mode == AlphaMode::Blend ? m_blend_pipeline.shader : m_g_pipeline.shader;

		material.uniform_set = m_graphics_controller.uniform_set_create(shader_id, 1, uniforms.data(), (uint32_t)uniforms.size());

		m_materials.push_back(material);
	}

	return first_material_offset;
}

VertexBufferId Renderer::vertex_buffer_create(const Vertex* data, size_t count) {
	m_vertex_buffers.push_back(m_graphics_controller.vertex_buffer_create(data, count * sizeof(Vertex)));

	return m_vertex_buffers.size() - 1;
}

IndexBufferId Renderer::index_buffer_create(const uint32_t* data, size_t count) {
	m_index_buffers.push_back(m_graphics_controller.index_buffer_create(data, count * 4, IndexType::Uint32));

	return m_index_buffers.size() - 1;
}

void Renderer::draw_skybox(SkyboxId skybox_id) {
	m_draw_list.skybox = skybox_id;
}

SkyboxId Renderer::skybox_create(const ImageSpecs& texture) {
	ImageInfo skybox_texture_info = {
		.usage = ImageUsageColorSampled | ImageUsageTransferDst,
		.view_type = ImageViewType::Cube,
		.format = Format::RGBA8_SRGB,
		.width = (uint32_t)texture.height,
		.height = (uint32_t)texture.width / 6,
		.depth = 1,
		.layer_count = 6
	};

	Skybox skybox{
		.image = m_graphics_controller.image_create(texture.data, skybox_texture_info)
	};

	RenderId texture_ids[2] = { skybox.image, m_skybox_pipeline.sampler };

	UniformInfo skybox_texture_uniform{
		.type = UniformType::CombinedImageSampler,
		.image_usage = ImageUsageColorSampled,
		.binding = 0,
		.ids = texture_ids,
		.id_count = 2
	};

	skybox.uniform_set_1 = m_graphics_controller.uniform_set_create(m_skybox_pipeline.shader, 1, &skybox_texture_uniform, 1);

	m_skyboxes.push_back(skybox);
	return (RenderId)m_skyboxes.size() - 1;
}