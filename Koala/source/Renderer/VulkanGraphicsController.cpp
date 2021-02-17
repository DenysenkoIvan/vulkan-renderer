#include "VulkanGraphicsController.h"

#include <spirv_reflect.h>

#include <algorithm>
#include <utility>
#include <stdexcept>

static uint32_t vk_format_to_size(VkFormat format) {
	switch (format) {
	case VK_FORMAT_R8G8B8A8_SRGB:		return 4 * 1;
	case VK_FORMAT_R32_SFLOAT:			return 1 * 4;
	case VK_FORMAT_R32G32_SFLOAT:		return 2 * 4;
	case VK_FORMAT_R32G32B32_SFLOAT:	return 3 * 4;
	case VK_FORMAT_R32G32B32A32_SFLOAT: return 4 * 4;
	case VK_FORMAT_D24_UNORM_S8_UINT:	return 1 * 3 + 1 * 1;
	case VK_FORMAT_D32_SFLOAT:			return 1;
	case VK_FORMAT_D32_SFLOAT_S8_UINT:	return 1 * 4 + 1 * 1;
	}

	throw std::runtime_error("Unknown format");
}

static bool format_has_stencil(VkFormat format) {
	switch (format) {
	case VK_FORMAT_S8_UINT:
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return true;
	default:
		return false;
	}
}

static std::pair<VkPipelineStageFlags, VkAccessFlags> image_layout_to_pipeline_stages_and_access(VkImageLayout layout) {
	VkPipelineStageFlags stages = 0;
	VkAccessFlags access = 0;
	
	if (layout == VK_IMAGE_LAYOUT_UNDEFINED) {
		stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	} else if (layout == VK_IMAGE_LAYOUT_GENERAL) {
		stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		access =
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT |
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
			VK_ACCESS_HOST_READ_BIT;
	} else if (layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
		stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	} else if (layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		stages = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	} else if (layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
		stages = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		access = VK_ACCESS_MEMORY_READ_BIT;
	} else if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	} else if (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL || layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
		access = VK_ACCESS_MEMORY_READ_BIT;
	} else if (layout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
		throw std::runtime_error("Image layout not supported");
	} else {
		throw std::runtime_error("Image layout not supported");
	}

	return std::make_pair(stages, access);
}

static std::pair<VkPipelineStageFlags, VkAccessFlags> buffer_usage_to_pipeline_stages_and_access(VkBufferUsageFlags usage) {
	VkPipelineStageFlags stages = 0;
	VkAccessFlags access = 0;

	if (usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) {
		stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		access |= VK_ACCESS_TRANSFER_READ_BIT;
	}
	if (usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) {
		stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		access |= VK_ACCESS_TRANSFER_WRITE_BIT;
	}
	if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
		stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		access |= VK_ACCESS_UNIFORM_READ_BIT;
	}
	if (usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) {
		stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		access |= VK_ACCESS_INDEX_READ_BIT;
	}
	if (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
		stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	}

	return std::make_pair(stages, access);
}

static VkImageLayout image_usage_to_optimal_image_layout(ImageUsageFlags usage) {
	if (usage & ImageUsageColorAttachment)
		return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	else if (usage & ImageUsageDepthStencilAttachment)
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	else if (usage & ImageUsageSampled)
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	else if (usage & ImageUsageCPUVisible)
		return VK_IMAGE_LAYOUT_GENERAL;
	else
		return VK_IMAGE_LAYOUT_GENERAL;
}

static VkImageAspectFlags vk_format_to_aspect(VkFormat format) {
	VkImageAspectFlags aspect = 0;

	switch (format) {
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
		aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	case VK_FORMAT_D32_SFLOAT:
		aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	default:
		aspect |= VK_IMAGE_ASPECT_COLOR_BIT;
	}

	return aspect;
}

static VkPipelineStageFlags image_usage_to_pipeline_stage(ImageUsageFlags usage) {
	VkPipelineStageFlags stages = 0;

	if (usage & ImageUsageTransferSrc)
		stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
	if (usage & ImageUsageTransferDst)
		stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
	if (usage & ImageUsageCPUVisible)
		stages |= VK_PIPELINE_STAGE_HOST_BIT;
	if (usage & ImageUsageSampled)
		stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	if (usage & ImageUsageDepthStencilAttachment)
		stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	if (usage & ImageUsageColorAttachment)
		stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	return stages;
}

/*static VkAccessFlags get_access_flags(VkImageLayout layout) {
	switch (layout) {
	case VK_IMAGE_LAYOUT_UNDEFINED:
		return 0;
	case VK_IMAGE_LAYOUT_GENERAL:
		return
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT |
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
			VK_ACCESS_HOST_READ_BIT;
	//case VK_IMAGE_LAYOUT_PREINITIALIZED:
	//	return VK_ACCESS_HOST_WRITE_BIT;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		return
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		return VK_ACCESS_TRANSFER_READ_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		return VK_ACCESS_TRANSFER_WRITE_BIT;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		return 0;
	}

	throw std::runtime_error("Image layout not supported");
}*/

static VkPipelineStageFlags get_pipeline_stage_flags(VkImageLayout layout) {
	switch (layout) {
	case VK_IMAGE_LAYOUT_UNDEFINED:
		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	case VK_IMAGE_LAYOUT_GENERAL:
		return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		return VK_PIPELINE_STAGE_HOST_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		return VK_PIPELINE_STAGE_TRANSFER_BIT;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		return
			// TODO: Features not enabled
			//VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
			//VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
			//VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}

	throw std::runtime_error("Image layout not supported");
}

void VulkanGraphicsController::create(VulkanContext* context) {
	m_context = context;

	uint32_t frame_count = m_context->swapchain_image_count() + 1;
	m_frames.resize(frame_count);

	VkCommandPoolCreateInfo command_pool_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_context->graphics_queue_index()
	};

	VkCommandBufferAllocateInfo command_buffer_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	VkFenceCreateInfo fence_info{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	VkDevice device = m_context->device();
	for (uint32_t i = 0; i < frame_count; i++) {
		if (vkCreateCommandPool(device, &command_pool_info, nullptr, &m_frames[i].command_pool) != VK_SUCCESS)
			throw std::runtime_error("Failed to create command pool");
		
		command_buffer_info.commandPool = m_frames[i].command_pool;
		if (vkAllocateCommandBuffers(device, &command_buffer_info, &m_frames[i].setup_buffer) != VK_SUCCESS ||
			vkAllocateCommandBuffers(device, &command_buffer_info, &m_frames[i].draw_buffer) != VK_SUCCESS)
			throw std::runtime_error("Failed to allocate command buffers");
	}

	m_frame_index = 0;
	VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	vkBeginCommandBuffer(m_frames[m_frame_index].setup_buffer, &begin_info);
	vkBeginCommandBuffer(m_frames[m_frame_index].draw_buffer, &begin_info);
}

void VulkanGraphicsController::destroy() {
	m_context->sync();

	vkEndCommandBuffer(m_frames[m_frame_index].setup_buffer);
	vkEndCommandBuffer(m_frames[m_frame_index].draw_buffer);

	VkDevice device = m_context->device();

	for (Buffer& buffer : m_buffers) {
		vkDestroyBuffer(device, buffer.buffer, nullptr);
		vkFreeMemory(device, buffer.memory, nullptr);
	}
	m_buffers.clear();

	for (Image& image : m_images) {
		vkDestroyImageView(device, image.view, nullptr);
		vkDestroyImage(device, image.image, nullptr);
		vkFreeMemory(device, image.memory, nullptr);
	}
	m_images.clear();

	for (Sampler& sampler : m_samplers)
		vkDestroySampler(device, sampler.sampler, nullptr);

	for (Shader& shader : m_shaders) {
		ShaderInfo& info = *shader.info;

		for (VkDescriptorSetLayout set_layout : info.set_layouts)
			vkDestroyDescriptorSetLayout(device, set_layout, nullptr);

		vkDestroyShaderModule(device, info.vertex_module, nullptr);
		vkDestroyShaderModule(device, info.fragment_module, nullptr);

		vkDestroyPipelineLayout(device, shader.pipeline_layout, nullptr);
	}
	m_shaders.clear();

	for (Pipeline& pipeline : m_pipelines)
		vkDestroyPipeline(device, pipeline.pipeline, nullptr);
	m_pipelines.clear();

	for (Frame& frame : m_frames) {
		vkDestroyCommandPool(device, frame.command_pool, nullptr);

		for (StagingBuffer& buffer : frame.staging_buffers) {
			vkDestroyBuffer(device, buffer.buffer, nullptr);
			vkFreeMemory(device, buffer.memory, nullptr);
		}
	}
	m_frames.clear();

	for (Framebuffer& framebuffer : m_framebuffers)
		vkDestroyFramebuffer(device, framebuffer.framebuffer, nullptr);
	m_framebuffers.clear();

	for (RenderPass& render_pass : m_render_passes)
		vkDestroyRenderPass(device, render_pass.render_pass, nullptr);
	m_render_passes.clear();
}

void VulkanGraphicsController::resize(uint32_t width, uint32_t height) {
	
	m_context->resize(width, height);

}

void VulkanGraphicsController::end_frame() {
	vkEndCommandBuffer(m_frames[m_frame_index].setup_buffer);
	vkEndCommandBuffer(m_frames[m_frame_index].draw_buffer);

	m_context->swap_buffers(m_frames[m_frame_index].setup_buffer, m_frames[m_frame_index].draw_buffer);

	m_frame_index = (m_frame_index + 1) % m_frames.size();

	VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	vkBeginCommandBuffer(m_frames[m_frame_index].setup_buffer, &begin_info);
	vkBeginCommandBuffer(m_frames[m_frame_index].draw_buffer, &begin_info);

	VkDevice device = m_context->device();
	for (StagingBuffer& buffer : m_frames[m_frame_index].staging_buffers) {
		vkDestroyBuffer(device, buffer.buffer, nullptr);
		vkFreeMemory(device, buffer.memory, nullptr);
	}
	m_frames[m_frame_index].staging_buffers.clear();
}

void VulkanGraphicsController::draw_begin(FramebufferId framebuffer_id, const glm::vec4* clear_colors, uint32_t count) {
	const Framebuffer& framebuffer = m_framebuffers[framebuffer_id];
	const RenderPass& render_pass = m_render_passes[framebuffer.render_pass_id];

	// Transition attchment image layout to its initial layout specified in VkRenderPass instance if it changed
	for (uint32_t i = 0; i < framebuffer.attachments.size(); i++) {
		Image& attachment_image = m_images[framebuffer.attachments[i]];

		image_should_have_layout(attachment_image, render_pass.attachments[i].initial_layout);
	}

	VkRenderPassBeginInfo render_pass_begin_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = framebuffer.render_pass,
		.framebuffer = framebuffer.framebuffer,
		.renderArea = { { 0, 0 }, framebuffer.extent },
		.clearValueCount = count,
		.pClearValues = (VkClearValue*)clear_colors
	};
	
	vkCmdBeginRenderPass(m_frames[m_frame_index].draw_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanGraphicsController::draw_end() {
	vkCmdEndRenderPass(m_frames[m_frame_index].draw_buffer);
}

void VulkanGraphicsController::draw_begin_for_screen(const glm::vec4& clear_color) {
	VkClearValue clear_value{ clear_color.r, clear_color.g, clear_color.b, clear_color.a };
	
	VkRenderPassBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = m_context->swapchain_render_pass(),
		.framebuffer = m_context->swapchain_framebuffer(),
		.renderArea = { {0, 0}, m_context->swapchain_extent() },
		.clearValueCount = 1,
		.pClearValues = &clear_value
	};
	
	vkCmdBeginRenderPass(m_frames[m_frame_index].draw_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanGraphicsController::draw_end_for_screen() {
	vkCmdEndRenderPass(m_frames[m_frame_index].draw_buffer);
}

void VulkanGraphicsController::draw_set_viewport(float x, float y, float width, float height, float min_depth, float max_depth) {
	VkViewport viewport{
		.x = x,
		.y = y,
		.width = width,
		.height = height,
		.minDepth = min_depth,
		.maxDepth = max_depth
	};

	vkCmdSetViewport(m_frames[m_frame_index].draw_buffer, 0, 1, &viewport);
}

void VulkanGraphicsController::draw_set_scissor(int x_offset, int y_offset, uint32_t width, uint32_t height) {
	VkRect2D scissor{
		.offset = { x_offset, y_offset },
		.extent = { width, height }
	};

	vkCmdSetScissor(m_frames[m_frame_index].draw_buffer, 0, 1, &scissor);
}

void VulkanGraphicsController::draw_bind_pipeline(PipelineId pipeline_id) {
	const Pipeline& pipeline = m_pipelines[pipeline_id];

	vkCmdBindPipeline(m_frames[m_frame_index].draw_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
}

void VulkanGraphicsController::draw_bind_vertex_buffer(BufferId buffer_id) {
	Buffer& buffer = m_buffers[buffer_id];

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(m_frames[m_frame_index].draw_buffer, 0, 1, &buffer.buffer, &offset);
}

void VulkanGraphicsController::draw_bind_index_buffer(BufferId buffer_id, IndexType index_type) {
	Buffer& buffer = m_buffers[buffer_id];

	vkCmdBindIndexBuffer(m_frames[m_frame_index].draw_buffer, buffer.buffer, 0, (VkIndexType)index_type);
}

void VulkanGraphicsController::draw_bind_uniform_sets(PipelineId pipeline_id, UniformSetId* set_ids, uint32_t count) {
	std::vector<VkDescriptorSet> descriptor_sets;
	descriptor_sets.reserve(count);
	
	for (uint32_t i = 0; i < count; i++) {
		UniformSet& set = m_uniform_sets[set_ids[i]];

		// Transition texture image layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		for (Uniform& uniform : set.uniforms) {
			if (uniform.type == UniformType::CombinedImageSampler) {
				for (size_t i = 0; i < uniform.ids.size(); i += 2)
					image_should_have_layout(m_images[uniform.ids[i]], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			} else if (uniform.type == UniformType::SampledImage) {
				for (ImageId id : uniform.ids)
					image_should_have_layout(m_images[id], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
		}
	
		descriptor_sets.push_back(set.descriptor_set);
	}

	vkCmdBindDescriptorSets(
		m_frames[m_frame_index].draw_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_pipelines[pipeline_id].layout,
		0, count, descriptor_sets.data(),
		0, nullptr
	);
}

void VulkanGraphicsController::draw_draw_indexed(uint32_t index_count) {
	vkCmdDrawIndexed(m_frames[m_frame_index].draw_buffer, index_count, 1, 0, 0, 0);
}

RenderPassId VulkanGraphicsController::render_pass_create(const RenderPassAttachment* attachments, uint32_t count) {
	RenderPass render_pass;
	render_pass.attachments.reserve(count);

	std::vector<VkAttachmentDescription> attachment_descriptions;
	attachment_descriptions.reserve(count);
	std::vector<VkAttachmentReference> color_attachments;
	std::vector<VkAttachmentReference> depth_stencil_attachments;
	// TODO: Resolve attachment support
	std::vector<VkAttachmentReference> resolve_attachments;

	VkSubpassDependency external_to_subpass{
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0
	};
	VkSubpassDependency subpass_to_external{
		.srcSubpass = 0,
		.dstSubpass = VK_SUBPASS_EXTERNAL
	};

	// TODO: make subpass dependencis optimal
	for (uint32_t i = 0; i < count; i++) {
		const RenderPassAttachment& attachment = attachments[i];

		InitialAction init_action = attachments[i].initial_action;
		FinalAction final_action = attachments[i].final_action;

		VkAttachmentDescription attachment_description{
			.format = (VkFormat)attachment.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = (VkAttachmentLoadOp)init_action,
			.storeOp = (VkAttachmentStoreOp)final_action,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
		};

		VkAttachmentReference attachment_reference{
			.attachment = i,
			.layout = (attachment.usage & ImageUsageColorAttachment) ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		};
	
		RenderPassAttachmentInfo render_pass_attachment_info{
			.attachment = attachment
		};

		if (attachment.usage & ImageUsageSampled) {
			VkPipelineStageFlags stages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT;

			external_to_subpass.srcStageMask |= stages;
			external_to_subpass.srcAccessMask |= access;

			subpass_to_external.dstStageMask |= stages;
			subpass_to_external.dstAccessMask |= access;
		}
		if (attachment.usage & ImageUsageTransferSrc) {
			VkPipelineStageFlags stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
			VkAccessFlags access = VK_ACCESS_TRANSFER_READ_BIT;

			external_to_subpass.srcStageMask |= stages;
			external_to_subpass.srcAccessMask |= access;

			subpass_to_external.dstStageMask |= stages;
			subpass_to_external.dstAccessMask |= access;
		}
		if (attachment.usage & ImageUsageTransferDst) {
			VkPipelineStageFlags stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
			VkAccessFlags access = VK_ACCESS_TRANSFER_WRITE_BIT;

			external_to_subpass.srcStageMask |= stages;
			external_to_subpass.srcAccessMask |= access;

			subpass_to_external.dstStageMask |= stages;
			subpass_to_external.dstAccessMask |= access;
		}
		if (attachment.usage & ImageUsageCPUVisible) {
			VkPipelineStageFlags stages = VK_PIPELINE_STAGE_HOST_BIT;
			VkAccessFlags access = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;

			external_to_subpass.srcStageMask |= stages;
			external_to_subpass.srcStageMask |= access;

			subpass_to_external.dstStageMask |= stages;
			subpass_to_external.dstAccessMask |= access;
		}

		if (attachment.usage & ImageUsageColorAttachment) {
			VkPipelineStageFlags stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			VkAccessFlags access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			external_to_subpass.srcStageMask |= stages;
			external_to_subpass.srcAccessMask |= access;
			external_to_subpass.dstStageMask |= stages;
			external_to_subpass.dstAccessMask |= access;

			subpass_to_external.srcStageMask |= stages;
			subpass_to_external.srcAccessMask |= access;
			subpass_to_external.dstStageMask |= stages;
			subpass_to_external.dstAccessMask |= access;

			if (init_action == InitialAction::Clear || init_action == InitialAction::DontCare) {
				attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

				render_pass_attachment_info.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
			} else {
				if (attachment.usage & ImageUsageSampled) {
					attachment_description.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

					render_pass_attachment_info.initial_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				} else {
					attachment_description.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

					render_pass_attachment_info.initial_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				}
			}

			if (final_action == FinalAction::DontCare) {
				attachment_description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				render_pass_attachment_info.final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			} else {
				if (attachment.usage & ImageUsageSampled) {
					attachment_description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

					render_pass_attachment_info.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				} else {
					attachment_description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

					render_pass_attachment_info.final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				}
			}

			color_attachments.push_back(attachment_reference);
		} else if (attachment.usage & ImageUsageDepthStencilAttachment) {
			VkPipelineStageFlags stages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			VkAccessFlags access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			external_to_subpass.srcStageMask |= stages;
			external_to_subpass.srcAccessMask |= access;
			external_to_subpass.dstStageMask |= stages;
			external_to_subpass.dstAccessMask |= access;

			subpass_to_external.srcStageMask |= stages;
			subpass_to_external.srcAccessMask |= access;
			subpass_to_external.dstStageMask |= stages;
			subpass_to_external.dstAccessMask |= access;

			if (format_has_stencil((VkFormat)attachment.format)) {
				attachment_description.stencilLoadOp = (VkAttachmentLoadOp)init_action;
				attachment_description.stencilStoreOp = (VkAttachmentStoreOp)final_action;
			}
			
			if (init_action == InitialAction::Clear || init_action == InitialAction::DontCare) {
				attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

				render_pass_attachment_info.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
			} else {
				if (attachment.usage & ImageUsageSampled) {
					attachment_description.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

					render_pass_attachment_info.initial_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				} else {
					attachment_description.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

					render_pass_attachment_info.initial_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				}
			}
			
			if (final_action == FinalAction::DontCare) {
				attachment_description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				render_pass_attachment_info.final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			} else {
				if (attachment.usage & ImageUsageSampled) {
					attachment_description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

					render_pass_attachment_info.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				} else {
					attachment_description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

					render_pass_attachment_info.final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				}
			}

			depth_stencil_attachments.push_back(attachment_reference);
		} else
			throw std::runtime_error("Invalid image usage");

		attachment_descriptions.push_back(attachment_description);
		render_pass.attachments.push_back(render_pass_attachment_info);
	}

	if (depth_stencil_attachments.size() != 1) throw std::runtime_error("Render pass supports only one depth stencil attachment");

	VkSubpassDescription subpass{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = (uint32_t)color_attachments.size(),
		.pColorAttachments = color_attachments.data(),
		.pResolveAttachments = resolve_attachments.data(),
		.pDepthStencilAttachment = depth_stencil_attachments.data()
	};

	VkSubpassDependency dependencies[2] = { external_to_subpass, subpass_to_external };

	VkRenderPassCreateInfo render_pass_info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = (uint32_t)attachment_descriptions.size(),
		.pAttachments = attachment_descriptions.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 2,
		.pDependencies = dependencies
	};
	
	if (vkCreateRenderPass(m_context->device(), &render_pass_info, nullptr, &render_pass.render_pass) != VK_SUCCESS)
		throw std::runtime_error("Failed to create framebuffer render pass");

	m_render_passes.push_back(render_pass);
	return (RenderPassId)m_render_passes.size() - 1;
}

FramebufferId VulkanGraphicsController::framebuffer_create(RenderPassId render_pass_id, const ImageId* ids, uint32_t count) {
	RenderPass& render_pass = m_render_passes[render_pass_id];

	uint32_t width = m_images[ids[0]].extent.width;
	uint32_t height = m_images[ids[0]].extent.height;

	Framebuffer framebuffer{
		.render_pass_id = render_pass_id,
		.render_pass = render_pass.render_pass,
		.extent = { width, height }
	};

	std::vector<VkImageView> views;
	views.reserve(count);

	framebuffer.attachments.reserve(count);
	for (uint32_t i = 0; i < count; i++) {
		framebuffer.attachments.push_back(ids[i]);
		views.push_back(m_images[ids[i]].view);
	}

	VkFramebufferCreateInfo framebuffer_info{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = framebuffer.render_pass,
		.attachmentCount = count,
		.pAttachments = views.data(),
		.width = width,
		.height = height,
		.layers = 1
	};
	
	if (vkCreateFramebuffer(m_context->device(), &framebuffer_info, nullptr, &framebuffer.framebuffer) != VK_SUCCESS)
		throw std::runtime_error("Failed to create framebuffer");

	m_framebuffers.push_back(std::move(framebuffer));
	return (FramebufferId)m_framebuffers.size() - 1;
}

ShaderId VulkanGraphicsController::shader_create(const std::vector<uint8_t>& vertex_spv, const std::vector<uint8_t>& fragment_spv) {
	m_shaders.push_back({});
	Shader& shader = m_shaders.back();
	shader.info = std::make_unique<ShaderInfo>();

	auto reflect_shader_stage = [this, &shader](const std::vector<uint8_t>& spv) {
		spv_reflect::ShaderModule shader_module(spv);

		VkShaderStageFlags shader_stage = (VkShaderStageFlags)shader_module.GetShaderStage();

		// Reflect Input Variables
		if (shader_stage == VK_SHADER_STAGE_VERTEX_BIT) {
			// Save vertex entry name
			shader.info->vertex_entry = shader_module.GetEntryPointName();

			uint32_t input_var_count = 0;
			shader_module.EnumerateInputVariables(&input_var_count, nullptr);
			std::vector<SpvReflectInterfaceVariable*> input_vars(input_var_count);
			shader_module.EnumerateInputVariables(&input_var_count, input_vars.data());

			shader.info->attribute_descriptions.reserve(input_var_count);

			uint32_t stride = 0;
			for (uint32_t i = 0; i < input_var_count; i++) {	
				SpvReflectInterfaceVariable* input_var = input_vars[i];

				VkVertexInputAttributeDescription attribute_description{
					.location = input_var->location,
					.binding = 0,
					.format = (VkFormat)input_var->format,
					.offset = stride
				};
				
				stride += vk_format_to_size((VkFormat)input_var->format);

				shader.info->attribute_descriptions.push_back(attribute_description);
			}

			shader.info->binding_description.binding = 0;
			shader.info->binding_description.stride = stride;
		} else {
			// Save fragment entry name
			shader.info->fragment_entry = shader_module.GetEntryPointName();
		}

		// Relfect Uniforms
		uint32_t binding_count = 0;
		shader_module.EnumerateDescriptorBindings(&binding_count, nullptr);
		std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
		shader_module.EnumerateDescriptorBindings(&binding_count, bindings.data());

		for (SpvReflectDescriptorBinding* descriptor_binding : bindings) {
			uint32_t set_idx = descriptor_binding->set;
			uint32_t binding_idx = descriptor_binding->binding;
			VkDescriptorType type = (VkDescriptorType)descriptor_binding->descriptor_type;
			uint32_t count = descriptor_binding->count;

			auto set_it = shader.info->find_set(set_idx);
			if (set_it == shader.info->sets.end()) {
				Set set;
				set.set = set_idx;
				shader.info->sets.push_back(set);
				set_it = shader.info->sets.end() - 1;
			}

			auto binding_it = set_it->find_binding(binding_idx);
			// Insert new binding, no problem
			if (binding_it == set_it->bindings.end()) {
				VkDescriptorSetLayoutBinding layout_binding{
					.binding = binding_idx,
					.descriptorType = type,
					.descriptorCount = count,
					.stageFlags = shader_stage
				};
				
				set_it->bindings.emplace_back(layout_binding);
			// Update binding stage and verify bindings are the same
			} else {
				binding_it->stageFlags |= shader_stage;

				if (type != binding_it->descriptorType)
					throw std::runtime_error("Uniform set binding redefinition with different descriptor type");
				if (count != binding_it->descriptorCount)
					throw std::runtime_error("Uniform set binding refefinition with different count");
			}
		}
	};

	reflect_shader_stage(vertex_spv);
	reflect_shader_stage(fragment_spv);

	// Create Vertex stage VkShaderModule
	VkShaderModuleCreateInfo vert_module_info{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = (uint32_t)vertex_spv.size(),
		.pCode = (uint32_t*)vertex_spv.data()
	};
	
	if (vkCreateShaderModule(m_context->device(), &vert_module_info, nullptr, &shader.info->vertex_module) != VK_SUCCESS)
		throw std::runtime_error("Failed to create Vertex stage VkShaderModule");

	// Create Fragment stage VkShaderModule
	VkShaderModuleCreateInfo frag_module_info{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = (uint32_t)fragment_spv.size(),
		.pCode = (uint32_t*)fragment_spv.data()
	};

	if (vkCreateShaderModule(m_context->device(), &frag_module_info, nullptr, &shader.info->fragment_module) != VK_SUCCESS)
		throw std::runtime_error("Failed to create Vertex stage VkShaderModule");

	// Create Vertex stage VkPipelineShaderStageCreateInfo
	shader.stage_create_infos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader.stage_create_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader.stage_create_infos[0].module = shader.info->vertex_module;
	shader.stage_create_infos[0].pName = shader.info->vertex_entry.c_str();

	// Create Fragment stage VkPipelineShaderStageCreateInfo
	shader.stage_create_infos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader.stage_create_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader.stage_create_infos[1].module = shader.info->fragment_module;
	shader.stage_create_infos[1].pName = shader.info->fragment_entry.c_str();

	// Create VkPipelineVertexInputStateCreateInfo
	shader.vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	shader.vertex_input_create_info.vertexBindingDescriptionCount = 1;
	shader.vertex_input_create_info.pVertexBindingDescriptions = &shader.info->binding_description;
	shader.vertex_input_create_info.vertexAttributeDescriptionCount = (uint32_t)shader.info->attribute_descriptions.size();
	shader.vertex_input_create_info.pVertexAttributeDescriptions = shader.info->attribute_descriptions.data();

	std::sort(shader.info->sets.begin(), shader.info->sets.end(), [](const auto& s0, const auto& s1) {
		return s0.set < s1.set;
	});

	// Create VkDescriptorSetLayouts
	size_t set_count = shader.info->sets.size();

	shader.info->set_layouts.reserve(set_count);
	for (size_t i = 0; i < set_count; i++) {
		Set& set = shader.info->sets[i];

		std::sort(set.bindings.begin(), set.bindings.end(), [](const auto& b0, const auto& b1) {
			return b0.binding < b1.binding;
		});

		VkDescriptorSetLayoutCreateInfo set_layout_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = (uint32_t)set.bindings.size(),
			.pBindings = set.bindings.data()
		};
	
		VkDescriptorSetLayout set_layout;
		if (vkCreateDescriptorSetLayout(m_context->device(), &set_layout_info, nullptr, &set_layout) != VK_SUCCESS)
			throw std::runtime_error("Failed to create descritptor set layout");

		shader.info->set_layouts.push_back(set_layout);
	}

	// Create pipeline layout
	VkPipelineLayoutCreateInfo pipeline_layout_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = (uint32_t)shader.info->set_layouts.size(),
		.pSetLayouts = shader.info->set_layouts.data()
	};

	if (vkCreatePipelineLayout(m_context->device(), &pipeline_layout_info, nullptr, &shader.pipeline_layout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create pipelien layout");

	return (ShaderId)m_shaders.size() - 1;
}

PipelineId VulkanGraphicsController::pipeline_create(const PipelineInfo* pipeline_info) {
	m_pipelines.push_back({});
	Pipeline& pipeline = m_pipelines.back();
	pipeline.info = *pipeline_info;

	const Shader& shader = m_shaders[pipeline.info.shader_id];
	
	pipeline.layout = shader.pipeline_layout;

	VkPipelineInputAssemblyStateCreateInfo assembly_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = (VkPrimitiveTopology)pipeline_info->assembly.topology,
		.primitiveRestartEnable = pipeline_info->assembly.restart_enable
	};

	VkViewport viewport{
		.x = 0,
		.y = 0,
		.width = (float)m_context->swapchain_extent().width,
		.height = (float)m_context->swapchain_extent().height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	VkRect2D scissor{
		.offset = { 0, 0 },
		.extent = m_context->swapchain_extent()
	};

	VkPipelineViewportStateCreateInfo viewport_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};

	VkPipelineRasterizationStateCreateInfo rasterization_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = pipeline_info->raster.depth_clamp_enable,
		.rasterizerDiscardEnable = pipeline_info->raster.rasterizer_discard_enable,
		.polygonMode = (VkPolygonMode)pipeline_info->raster.polygon_mode,
		.cullMode = (VkCullModeFlags)pipeline_info->raster.cull_mode,
		.frontFace = (VkFrontFace)pipeline_info->raster.front_face,
		.depthBiasEnable = pipeline_info->raster.depth_bias_enable,
		.depthBiasConstantFactor = pipeline_info->raster.depth_bias_constant_factor,
		.depthBiasClamp = pipeline_info->raster.depth_bias_clamp,
		.depthBiasSlopeFactor =  pipeline_info->raster.detpth_bias_slope_factor,
		.lineWidth = pipeline_info->raster.line_width
	};

	VkPipelineMultisampleStateCreateInfo multisample_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE
	};

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE
	};
	
	VkPipelineColorBlendAttachmentState blend_attachment{
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	VkPipelineColorBlendStateCreateInfo color_blend_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &blend_attachment,
		.blendConstants = { 0, 0, 0, 0 }
	};

	VkPipelineDynamicStateCreateInfo dynamic_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = pipeline_info->dynamic_states.dynamic_state_count,
		.pDynamicStates = (VkDynamicState*)pipeline_info->dynamic_states.dynamic_states
	};

	VkGraphicsPipelineCreateInfo pipeline_create_info{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = (uint32_t)shader.stage_create_infos.size(),
		.pStages = shader.stage_create_infos.data(),
		.pVertexInputState = &shader.vertex_input_create_info,
		.pInputAssemblyState = &assembly_state,
		.pTessellationState = nullptr,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterization_state,
		.pMultisampleState = &multisample_state,
		.pDepthStencilState = &depth_stencil_state,
		.pColorBlendState = &color_blend_state,
		// TODO: Add dynamic states support
		.pDynamicState = &dynamic_state,
		.layout = shader.pipeline_layout,
		.renderPass = pipeline_info->render_pass_id
			? m_render_passes[pipeline_info->render_pass_id.value()].render_pass
			: m_context->swapchain_render_pass(),
		.subpass = 0
	};
	
	if (vkCreateGraphicsPipelines(m_context->device(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline.pipeline) != VK_SUCCESS)
		throw std::runtime_error("Failed to create graphics pipeline");

	return (PipelineId)m_pipelines.size() - 1;
}

BufferId VulkanGraphicsController::vertex_buffer_create(const void* data, size_t size) {
	Buffer buffer{
		.size = size,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
	};
	
	buffer.buffer = buffer_create(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size);
	buffer.memory = buffer_allocate(buffer.buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	buffer_copy(buffer.buffer, data, size);

	buffer_memory_barrier(buffer.buffer, buffer.usage, 0, size);

	m_buffers.push_back(buffer);
	return (BufferId)m_buffers.size() - 1;
}

BufferId VulkanGraphicsController::index_buffer_create(const void* data, size_t size, IndexType index_type) {
	Buffer buffer{
		.size = size,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT
	};
	buffer.index.index_type = (VkIndexType)index_type;
	buffer.index.index_count = index_type == IndexType::Uint16 ? (uint32_t)size / 2 : (uint32_t)size / 4;

	buffer.buffer = buffer_create(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size);
	buffer.memory = buffer_allocate(buffer.buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	buffer_copy(buffer.buffer, data, size);

	buffer_memory_barrier(buffer.buffer, buffer.usage, 0, size);

	m_buffers.push_back(buffer);
	return (BufferId)m_buffers.size() - 1;
}

BufferId VulkanGraphicsController::uniform_buffer_create(const void* data, size_t size) {
	Buffer buffer{
		.size = size,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
	};

	buffer.buffer = buffer_create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size);
	buffer.memory = buffer_allocate(buffer.buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (data) {
		buffer_copy(buffer.buffer, data, size);
		buffer_memory_barrier(buffer.buffer, buffer.usage, 0, size);
	}

	m_buffers.push_back(buffer);
	return (BufferId)m_buffers.size() - 1;
}

void VulkanGraphicsController::buffer_update(BufferId buffer_id, const void* data) {
	Buffer& buffer = m_buffers[buffer_id];

	buffer_memory_barrier(buffer.buffer, buffer.usage, 0, buffer.size);

	buffer_copy(buffer.buffer, data, buffer.size);

	buffer_memory_barrier(buffer.buffer, buffer.usage, 0, buffer.size);
}

ImageId VulkanGraphicsController::image_create(const void* data, ImageUsageFlags usage, Format format, uint32_t width, uint32_t height) {
	VkFormat vk_format = (VkFormat)format;
	VkImageAspectFlags aspect = 0;
	VkImageUsageFlags vk_usage = 0;
	VkDeviceSize size = vk_format_to_size(vk_format) * width * height;

	if (usage & ImageUsageDepthStencilAttachment) {
		aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;

		if (format_has_stencil(vk_format))
			aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	} else
		aspect = VK_IMAGE_ASPECT_COLOR_BIT;

	VkMemoryPropertyFlags mem_props = usage & ImageUsageCPUVisible
		? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		: VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	Image image{
		.image = vulkan_image_create({ width, height }, vk_format, (VkImageUsageFlags)usage),
		.memory = vulkan_image_allocate(image.image, mem_props),
		.view = vulkan_image_view_create(image.image, vk_format, aspect),
		.extent = { width, height },
		.format = vk_format,
		.usage = usage,
		.current_layout = VK_IMAGE_LAYOUT_UNDEFINED,
		.aspect = aspect
	};

	m_images.push_back(image);
	ImageId image_id = (ImageId)m_images.size() - 1;

	if (data)
		image_update(image_id, data, size);

	return image_id;
}

void VulkanGraphicsController::image_update(ImageId image_id, const void* data, size_t size) {
	Image& image = m_images[image_id];

	image_layout_transition(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	
	vulkan_image_copy(image.image, image.extent, image.aspect, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, data, size);

	if (image.current_layout == VK_IMAGE_LAYOUT_UNDEFINED)
		image.current_layout = image_usage_to_optimal_image_layout(image.usage);

	image_layout_transition(image, image.current_layout);
}

SamplerId VulkanGraphicsController::sampler_create(const SamplerInfo& info) {
	VkSamplerCreateInfo sampler_info{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = (VkFilter)info.mag_filter,
		.minFilter = (VkFilter)info.min_filter,
		.mipmapMode = (VkSamplerMipmapMode)info.mip_map_mode,
		.addressModeU = (VkSamplerAddressMode)info.address_mode_u,
		.addressModeV = (VkSamplerAddressMode)info.address_mode_v,
		.addressModeW = (VkSamplerAddressMode)info.address_mode_w,
		.mipLodBias = info.mip_lod_bias,
		.anisotropyEnable = info.anisotropy_enable,
		.maxAnisotropy = info.max_anisotropy,
		.compareEnable = info.compare_enable,
		.compareOp = (VkCompareOp)info.comapare_op,
		.minLod = info.min_lod,
		.maxLod = info.max_lod,
		.borderColor = (VkBorderColor)info.border_color,
		.unnormalizedCoordinates = info.unnormalized_coordinates
	};

	Sampler sampler{
		.info = info
	};

	if (vkCreateSampler(m_context->device(), &sampler_info, nullptr, &sampler.sampler) != VK_SUCCESS)
		throw std::runtime_error("Failed to create sampler");

	m_samplers.push_back(sampler);

	return (SamplerId)m_samplers.size() - 1;
}

UniformSetId VulkanGraphicsController::uniform_set_create(ShaderId shader_id, uint32_t set_idx, const std::vector<Uniform>& uniforms) {
	Set& set = *m_shaders[shader_id].info->find_set(set_idx);

	std::vector<std::vector<VkDescriptorImageInfo>> image_infos_collector;
	std::vector<std::vector<VkDescriptorBufferInfo>> buffer_infos_collector;
	std::vector<VkWriteDescriptorSet> writes;

	DescriptorPoolKey pool_key;
	for (const Uniform& uniform : uniforms) {
		auto binding_it = set.find_binding(uniform.binding);
		
		if (binding_it == set.bindings.end())
			throw std::runtime_error("No binding found");

		VkWriteDescriptorSet write{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstBinding = binding_it->binding,
			.dstArrayElement = 0
		};

		switch (uniform.type) {
		case UniformType::Sampler: {
			throw std::runtime_error("UniformType not supported");
		}
		case UniformType::CombinedImageSampler: {
			std::vector<VkDescriptorImageInfo> image_infos;

			for (size_t i = 0; i < uniform.ids.size(); i += 2) {
				Image& image = m_images[uniform.ids[i]];

				image_should_have_layout(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

				VkDescriptorImageInfo image_info{
					.sampler = m_samplers[uniform.ids[i + 1]].sampler,
					.imageView = image.view,
					.imageLayout = image.current_layout
				};

				image_infos.push_back(image_info);
			}

			write.descriptorCount = (uint32_t)uniform.ids.size() / 2;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.pImageInfo = image_infos.data();

			image_infos_collector.push_back(std::move(image_infos));

			break;
		}
		case UniformType::SampledImage: {
			throw std::runtime_error("UniformType not supported");
			std::vector<VkDescriptorImageInfo> image_infos;

			for (size_t i = 0; i < uniform.ids.size(); i++) {
				Image& image = m_images[uniform.ids[i]];

				VkDescriptorImageInfo image_info{
					.imageView = image.view,
					.imageLayout = image.current_layout
				};

				image_infos.push_back(image_info);
			}

			write.descriptorCount = (uint32_t)uniform.ids.size();
			write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			write.pImageInfo = image_infos.data();

			image_infos_collector.push_back(std::move(image_infos));

			break;
		}
		case UniformType::UniformBuffer: {
			std::vector<VkDescriptorBufferInfo> buffer_infos;

			for (size_t i = 0; i < uniform.ids.size(); i++) {
				Buffer& buffer = m_buffers[uniform.ids[i]];

				VkDescriptorBufferInfo buffer_info{
					.buffer = buffer.buffer,
					.offset = 0,
					.range = VK_WHOLE_SIZE
				};

				buffer_infos.push_back(buffer_info);
			}

			write.descriptorCount = (uint32_t)uniform.ids.size();
			write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write.pBufferInfo = buffer_infos.data();
			
			buffer_infos_collector.push_back(std::move(buffer_infos));

			break;
		}
		}

		pool_key.uniform_type_counts[(uint32_t)uniform.type] = write.descriptorCount;

		writes.push_back(write);
	}

	uint32_t pool_idx = descriptor_pool_allocate(pool_key);
	const DescriptorPool& pool = m_descriptor_pools.at(pool_key)[pool_idx];

	VkDescriptorSetAllocateInfo set_allocate_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = pool.pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &m_shaders[shader_id].info->set_layouts[set_idx]
	};
	
	VkDescriptorSet descriptor_set;
	if (vkAllocateDescriptorSets(m_context->device(), &set_allocate_info, &descriptor_set) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate descriptor set");

	UniformSet uniform_set{
		.uniforms = uniforms,
		.pool_key = pool_key,
		.pool_idx = pool_idx,
		.shader = shader_id,
		.set_idx= set_idx,
		.descriptor_set = descriptor_set
	};

	for (VkWriteDescriptorSet& write : writes) {
		write.dstSet = descriptor_set;
	}
	vkUpdateDescriptorSets(m_context->device(), (uint32_t)writes.size(), writes.data(), 0, nullptr);

	m_uniform_sets.push_back(std::move(uniform_set));

	return (UniformSetId)m_uniform_sets.size() - 1;
}




VkBuffer VulkanGraphicsController::buffer_create(VkBufferUsageFlags usage, VkDeviceSize size) {
	VkBuffer buffer;

	VkBufferCreateInfo buffer_info{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	if (vkCreateBuffer(m_context->device(), &buffer_info, nullptr, &buffer) != VK_SUCCESS)
		throw std::runtime_error("Failed to create buffer");

	return buffer;
}

VkDeviceMemory VulkanGraphicsController::buffer_allocate(VkBuffer buffer, VkMemoryPropertyFlags mem_props) {
	VkMemoryRequirements mem_requirements;
	vkGetBufferMemoryRequirements(m_context->device(), buffer, &mem_requirements);

	VkMemoryAllocateInfo allocate_info{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_requirements.size,
		.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, mem_props)
	};

	VkDeviceMemory memory;
	if (vkAllocateMemory(m_context->device(), &allocate_info, nullptr, &memory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate buffer memory");

	vkBindBufferMemory(m_context->device(), buffer, memory, 0);

	return memory;
}

void VulkanGraphicsController::buffer_copy(VkBuffer buffer, const void* data, VkDeviceSize size) {
	VkBuffer staging_buffer = buffer_create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size);
	VkDeviceMemory staging_memory = buffer_allocate(staging_buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* staging_data = nullptr;
	vkMapMemory(m_context->device(), staging_memory, 0, size, 0, &staging_data);
	memcpy(staging_data, data, size);
	vkUnmapMemory(m_context->device(), staging_memory);

	VkBufferCopy region{
		.size = size
	};

	vkCmdCopyBuffer(m_frames[m_frame_index].draw_buffer, staging_buffer, buffer, 1, &region);

	m_frames[m_frame_index].staging_buffers.emplace_back(staging_buffer, staging_memory);
}

void VulkanGraphicsController::buffer_memory_barrier(VkBuffer& buffer, VkBufferUsageFlags usage, VkDeviceSize offset, VkDeviceSize size) {
	auto [src_stages, src_access] = buffer_usage_to_pipeline_stages_and_access(usage);
	auto [dst_stages, dst_access] = buffer_usage_to_pipeline_stages_and_access(usage);
	
	VkBufferMemoryBarrier barrier{
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.srcAccessMask = src_access,
		.dstAccessMask = dst_access,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = buffer,
		.offset = offset,
		.size = size
	};

	vkCmdPipelineBarrier(
		m_frames[m_frame_index].draw_buffer,
		src_stages, dst_stages,
		0,
		0, nullptr,
		1, &barrier,
		0, nullptr
	);
}

VkImage VulkanGraphicsController::vulkan_image_create(VkExtent2D extent, VkFormat format, VkImageUsageFlags usage) {
	VkImageCreateInfo image_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = { extent.width, extent.height, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	
	VkImage image;
	if (vkCreateImage(m_context->device(), &image_info, nullptr, &image) != VK_SUCCESS)
		throw std::runtime_error("Failed to create image");
	
	return image;
}

VkDeviceMemory VulkanGraphicsController::vulkan_image_allocate(VkImage image, VkMemoryPropertyFlags mem_props) {
	VkMemoryRequirements mem_reqs;
	vkGetImageMemoryRequirements(m_context->device(), image, &mem_reqs);
	
	VkMemoryAllocateInfo memory_info{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, mem_props)
	};

	VkDeviceMemory memory;
	if (vkAllocateMemory(m_context->device(), &memory_info, nullptr, &memory) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate image memory");

	vkBindImageMemory(m_context->device(), image, memory, 0);

	return memory;
}

VkImageView VulkanGraphicsController::vulkan_image_view_create(VkImage image, VkFormat format, VkImageAspectFlags aspect) {
	VkImageViewCreateInfo view_info{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange = { aspect, 0, 1, 0, 1 }
	};
	
	VkImageView image_view;
	if (vkCreateImageView(m_context->device(), &view_info, nullptr, &image_view) != VK_SUCCESS)
		throw std::runtime_error("Failed to create image view");

	return image_view;
}

void VulkanGraphicsController::vulkan_image_copy(VkImage image, VkExtent2D extent, VkImageAspectFlags aspect, VkImageLayout layout, const void* data, size_t size) {
	VkBuffer staging_buffer = buffer_create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size);
	VkDeviceMemory staging_memory = buffer_allocate(staging_buffer, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	void* staging_data = nullptr;
	vkMapMemory(m_context->device(), staging_memory, 0, size, 0, &staging_data);
	memcpy(staging_data, data, size);
	vkUnmapMemory(m_context->device(), staging_memory);

	VkBufferImageCopy region{
		.imageSubresource = { aspect, 0, 0, 1 },
		.imageExtent = { extent.width, extent.height, 1 }
	};

	vkCmdCopyBufferToImage(m_frames[m_frame_index].draw_buffer, staging_buffer, image, layout, 1, &region);

	m_frames[m_frame_index].staging_buffers.emplace_back(staging_buffer, staging_memory);
}

void VulkanGraphicsController::image_should_have_layout(Image& image, VkImageLayout layout) {
	if (image.current_layout != layout && layout != VK_IMAGE_LAYOUT_UNDEFINED)
		image_layout_transition(image, layout);
}

void VulkanGraphicsController::image_layout_transition(Image& image, VkImageLayout new_layout) {
	if (image.current_layout == new_layout)
		return;

	vulkan_image_memory_barrier(image.image, image.aspect, image.current_layout, new_layout);

	image.current_layout = new_layout;
}

void VulkanGraphicsController::vulkan_image_memory_barrier(VkImage image, VkImageAspectFlags aspect, VkImageLayout old_layout, VkImageLayout new_layout) {
	auto [src_stages, src_access] = image_layout_to_pipeline_stages_and_access(old_layout);
	auto [dst_stages, dst_access] = image_layout_to_pipeline_stages_and_access(new_layout);

	VkImageMemoryBarrier barrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = src_access,
		.dstAccessMask = dst_access,
		.oldLayout = old_layout,
		.newLayout = new_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = { aspect, 0, 1, 0, 1 }
	};

	vkCmdPipelineBarrier(
		m_frames[m_frame_index].draw_buffer,
		src_stages, dst_stages,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}

uint32_t VulkanGraphicsController::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
	const VkPhysicalDeviceMemoryProperties& mem_props = m_context->physical_device_mem_props();

	for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
		if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type");
}

uint32_t VulkanGraphicsController::descriptor_pool_allocate(const DescriptorPoolKey& key) {
	if (!m_descriptor_pools.contains(key)) {
		m_descriptor_pools[key] = {};
	}

	std::vector<DescriptorPool>& pools = m_descriptor_pools.at(key);

	for (uint32_t i = 0; i < pools.size(); i++) {
		if (pools[i].usage_count < MAX_SETS_PER_DESCRIPTOR_POOL) {
			pools[i].usage_count++;
			return i;
		}
	}
	
	std::vector<VkDescriptorPoolSize> sizes;

	if (key.uniform_type_counts[(uint32_t)UniformType::Sampler]) {
		VkDescriptorPoolSize size{
			.type = VK_DESCRIPTOR_TYPE_SAMPLER,
			.descriptorCount = key.uniform_type_counts[(uint32_t)UniformType::Sampler] * MAX_SETS_PER_DESCRIPTOR_POOL
		};

		sizes.push_back(size);
	}
	if (key.uniform_type_counts[(uint32_t)UniformType::CombinedImageSampler]) {
		VkDescriptorPoolSize size{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = key.uniform_type_counts[(uint32_t)UniformType::CombinedImageSampler] * MAX_SETS_PER_DESCRIPTOR_POOL
		};

		sizes.push_back(size);
	}
	if (key.uniform_type_counts[(uint32_t)UniformType::SampledImage]) {
		VkDescriptorPoolSize size{
			.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.descriptorCount = key.uniform_type_counts[(uint32_t)UniformType::SampledImage] * MAX_SETS_PER_DESCRIPTOR_POOL
		};

		sizes.push_back(size);
	}
	if (key.uniform_type_counts[(uint32_t)UniformType::UniformBuffer]) {
		VkDescriptorPoolSize size{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = key.uniform_type_counts[(uint32_t)UniformType::UniformBuffer] * MAX_SETS_PER_DESCRIPTOR_POOL
		};

		sizes.push_back(size);
	}

	VkDescriptorPoolCreateInfo pool_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = MAX_SETS_PER_DESCRIPTOR_POOL,
		.poolSizeCount = (uint32_t)sizes.size(),
		.pPoolSizes = sizes.data()
	};

	VkDescriptorPool pool;
	if (vkCreateDescriptorPool(m_context->device(), &pool_info, nullptr, &pool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create descriptor pool");

	DescriptorPool desc_pool{
		.pool = pool,
		.usage_count = 1
	};

	pools.push_back(desc_pool);

	return (uint32_t)pools.size() - 1;
}

void VulkanGraphicsController::descriptor_pools_free() {

}