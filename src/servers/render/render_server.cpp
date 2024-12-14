#include "render_server.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1000000
#include <vk_mem_alloc.h>

#include <cstdint>
#include <vector>

#include "nur.h"


RenderServer::RenderServer() {
	PRINT(bold | fg(blue), "Begun initialising RenderServer\n");

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		ERROR(SDL_GetError());
		ERROR_POPUP("SDL Initialization Error", SDL_GetError());
	}

	const bool fullscreen = false;
	const uint32_t monitor = 0;

	window_ = SDL_CreateWindow(GAME_NAME, static_cast<int>(window_extent_.width),
	                           static_cast<int>(window_extent_.height), SDL_WINDOW_VULKAN);
	if (window_ == nullptr) {
		ERROR(SDL_GetError());
		ERROR_POPUP("Window Creation Error", SDL_GetError());
	};

	PRINT("Created SDL Window:\n");
	PRINT("  Dimensions: {0} x {1}\n", window_extent_.width, window_extent_.height);
	PRINT("  Fullscreen: {0}\n", fullscreen);
	PRINT("  Monitor: {0}\n", monitor);

	vkb::InstanceBuilder instance_builder;
	auto inst_ret = instance_builder.set_app_name(GAME_NAME)
	                    .set_engine_name("nur")
	                    .set_app_version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH)
	                    .set_engine_version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH)
#ifdef DEBUG
	                    .request_validation_layers(true)
	                    .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
	                                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	                    .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
	                                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
	                                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
	                                              VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT)
	                    .set_debug_callback([](VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	                                           VkDebugUtilsMessageTypeFlagsEXT message_type,
	                                           const VkDebugUtilsMessengerCallbackDataEXT* debug_callback_data,
	                                           void* /*pUserData*/) -> VkBool32 {
		                    const auto* severity_string = vkb::to_string_message_severity(message_severity);
		                    const auto* type_string = vkb::to_string_message_type(message_type);
		                    const auto severity_color = [&]() {
			                    if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
				                    return fg(gray);
			                    }
			                    if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
				                    return fg(green);
			                    }
			                    if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
				                    return fg(yellow);
			                    }
			                    return fg(red) | bg(yellow) | bold;
		                    }();

		                    PRINT(fg(red), "Vulkan ");
		                    PRINT(severity_color, "{}", severity_string);
		                    PRINT(" {0}: {1}\n", type_string, debug_callback_data->pMessage);

		                    return VK_FALSE;
	                    })
#endif
	                    .build();

	if (!inst_ret) {
		ERROR(inst_ret.error().message().c_str());
		ERROR_POPUP("Vulkan Initialisation Error", inst_ret.error().message().c_str());
	};

	vkb::Instance vkb_instance = inst_ret.value();
	instance_ = vkb_instance.instance;
	debug_messenger_ = vkb_instance.debug_messenger;


	SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_);

	vkb::PhysicalDeviceSelector selector{vkb_instance};
	auto phys_ret = selector.set_surface(surface_).select();
	if (!phys_ret) {
		ERROR(phys_ret.error().message().c_str());
		ERROR_POPUP("Vulkan Physical Device Error", phys_ret.error().message().c_str());
	}
	const vkb::PhysicalDevice& vkb_physical_device = phys_ret.value();

	vkb::DeviceBuilder device_builder{vkb_physical_device};
	auto dev_ret = device_builder.build();
	if (!dev_ret) {
		ERROR(dev_ret.error().message().c_str());
		ERROR_POPUP("Vulkan Device Error", dev_ret.error().message().c_str());
	}
	const vkb::Device& vkb_device = dev_ret.value();

	device_ = vkb_device.device;
	physical_device_ = vkb_physical_device.physical_device;

	graphics_queue_ = vkb_device.get_queue(vkb::QueueType::graphics).value();
	graphics_queue_family_ = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

	// initialize the memory allocator
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = physical_device_;
    allocator_info.device = device_;
    allocator_info.instance = instance_;
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	allocator_info.vulkanApiVersion = VK_API_VERSION_1_0;
    vmaCreateAllocator(&allocator_info, &allocator_);

	VkCommandPoolCreateInfo command_pool_info = {};
	command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_info.pNext = nullptr;
	command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	command_pool_info.queueFamilyIndex = graphics_queue_family_;

	for (auto& frame : frames_) {
		vkCreateCommandPool(device_, &command_pool_info, nullptr, &frame.command_pool);
		deletion_lists_.command_pools.push_back(frame.command_pool);

		VkCommandBufferAllocateInfo cmd_alloc_info = {};
		cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_alloc_info.pNext = nullptr;
		cmd_alloc_info.commandPool = frame.command_pool;
		cmd_alloc_info.commandBufferCount = 1;
		cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		vkAllocateCommandBuffers(device_, &cmd_alloc_info, &frame.main_command_buffer);
	}

	VkAttachmentDescription color_attachment = {};

	vkb::SwapchainBuilder swapchain_builder{physical_device_, device_, surface_};

	vkb::Swapchain vkb_swapchain = swapchain_builder.use_default_format_selection()
	                                   .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
	                                   .set_desired_extent(window_extent_.width, window_extent_.height)
	                                   .build()
	                                   .value();

	swapchain_ = vkb_swapchain.swapchain;
	deletion_lists_.swapchains.push_back(swapchain_);

	swapchain_images_ = vkb_swapchain.get_images().value();

	swapchain_image_views_ = vkb_swapchain.get_image_views().value();
	for (auto* swapchain_image_view : swapchain_image_views_) {
		deletion_lists_.image_views.push_back(swapchain_image_view);
	}

	swapchain_image_format_ = vkb_swapchain.image_format;

	color_attachment.format = swapchain_image_format_;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;

	vkCreateRenderPass(device_, &render_pass_info, nullptr, &render_pass_);
	deletion_lists_.render_passes.push_back(render_pass_);

	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;

	fb_info.renderPass = render_pass_;
	fb_info.attachmentCount = 1;
	fb_info.width = window_extent_.width;
	fb_info.height = window_extent_.height;
	fb_info.layers = 1;

	const uint32_t swapchain_imagecount = swapchain_images_.size();
	framebuffers_ = std::vector<VkFramebuffer>(swapchain_imagecount);

	for (int i = 0; i < swapchain_imagecount; i++) {
		fb_info.pAttachments = &swapchain_image_views_[i];
		vkCreateFramebuffer(device_, &fb_info, nullptr, &framebuffers_[i]);
		deletion_lists_.framebuffers.push_back(framebuffers_[i]);
	}

	VkFenceCreateInfo fence_create_info = {};
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_create_info.pNext = nullptr;
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphore_create_info = {};
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphore_create_info.pNext = nullptr;
	semaphore_create_info.flags = 0;

	for (auto& frame : frames_) {
		vkCreateFence(device_, &fence_create_info, nullptr, &frame.render_fence);
		deletion_lists_.fences.push_back(frame.render_fence);

		vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &frame.swapchain_semaphore);
		deletion_lists_.semaphores.push_back(frame.swapchain_semaphore);

		vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &frame.render_semaphore);
		deletion_lists_.semaphores.push_back(frame.render_semaphore);
	}

	VkExtent3D draw_image_extent = {
		window_extent_.width,
		window_extent_.height,
		1
	};

	//hardcoding the draw format to 32 bit float
	draw_image_.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	draw_image_.imageExtent = draw_image_extent;

	VkImageUsageFlags draw_image_usages{};
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkFramebufferCreateInfo draw_fb_info = {};
	draw_fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	draw_fb_info.pNext = nullptr;
	draw_fb_info.renderPass = render_pass_; // Use the same render pass you created earlier
	draw_fb_info.attachmentCount = 1;      // Only one attachment, the draw image's image view
	draw_fb_info.pAttachments = &draw_image_.imageView;
	draw_fb_info.width = draw_image_.imageExtent.width;
	draw_fb_info.height = draw_image_.imageExtent.height;
	draw_fb_info.layers = 1;               // Single layer framebuffer
    deletion_lists_.framebuffers.push_back(draw_image_framebuffer_); // Ensure cleanup

	VkImageCreateInfo rimg_info = {};
    rimg_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    rimg_info.pNext = nullptr;
    rimg_info.imageType = VK_IMAGE_TYPE_2D;
    rimg_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    rimg_info.extent = draw_image_extent;
    rimg_info.mipLevels = 1;
    rimg_info.arrayLayers = 1;
    rimg_info.samples = VK_SAMPLE_COUNT_1_BIT;
    rimg_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    rimg_info.usage = draw_image_usages;

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_alloc_info = {};
	rimg_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_alloc_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(allocator_, &rimg_info, &rimg_alloc_info, &draw_image_.image, &draw_image_.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo rview_info = {};
    rview_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    rview_info.pNext = nullptr;
    rview_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    rview_info.image = draw_image_.image;
    rview_info.format = draw_image_.imageFormat;
    rview_info.subresourceRange.baseMipLevel = 0;
    rview_info.subresourceRange.levelCount = 1;
    rview_info.subresourceRange.baseArrayLayer = 0;
    rview_info.subresourceRange.layerCount = 1;
    rview_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	vkCreateImageView(device_, &rview_info, nullptr, &draw_image_.imageView);

	deletion_lists_.images.push_back(draw_image_.image);
	deletion_lists_.image_views.push_back(draw_image_.imageView);

	PRINT(bold | fg(blue), "Finished initialising RenderServer\n\n");
};

void RenderServer::draw() {
    vkWaitForFences(device_, 1, &get_current_frame().render_fence, true, UINT64_MAX);
    vkResetFences(device_, 1, &get_current_frame().render_fence);

    uint32_t swapchain_image_index;
    vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, get_current_frame().swapchain_semaphore, nullptr,
                          &swapchain_image_index);
    VkCommandBuffer cmd = get_current_frame().main_command_buffer;
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo cmd_begin_info = {};
    cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &cmd_begin_info);

    // Transition draw_image_ for rendering
    VkImageMemoryBarrier barrier_to_render = {};
    barrier_to_render.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_to_render.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier_to_render.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier_to_render.image = draw_image_.image;
    barrier_to_render.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_to_render.subresourceRange.baseMipLevel = 0;
    barrier_to_render.subresourceRange.levelCount = 1;
    barrier_to_render.subresourceRange.baseArrayLayer = 0;
    barrier_to_render.subresourceRange.layerCount = 1;
    barrier_to_render.srcAccessMask = 0;
    barrier_to_render.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &barrier_to_render);

    // Render pass targeting draw_image_
    VkClearValue clear_value;
    float flash = abs(sin(frame_number_ / 120.F));
    clear_value.color = {{0.0f, 0.0f, flash, 1.0f}};

    VkRenderPassBeginInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_info.renderPass = render_pass_; // Assuming render_pass_ is compatible with draw_image_
    rp_info.renderArea.offset = {.x=0, .y=0};
    rp_info.renderArea.extent = window_extent_;
    rp_info.framebuffer = draw_image_framebuffer_; // Framebuffer containing draw_image_
    rp_info.clearValueCount = 1;
    rp_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

    // [Perform rendering commands here targeting draw_image_]

    vkCmdEndRenderPass(cmd);

    // Transition draw_image_ for copy
    VkImageMemoryBarrier barrier_to_copy = barrier_to_render;
    barrier_to_copy.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier_to_copy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier_to_copy.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier_to_copy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &barrier_to_copy);

    // Transition swapchain image for copy
    VkImageMemoryBarrier swapchain_barrier = {};
    swapchain_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapchain_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchain_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchain_barrier.image = swapchain_images_[swapchain_image_index];
    swapchain_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    swapchain_barrier.subresourceRange.baseMipLevel = 0;
    swapchain_barrier.subresourceRange.levelCount = 1;
    swapchain_barrier.subresourceRange.baseArrayLayer = 0;
    swapchain_barrier.subresourceRange.layerCount = 1;
    swapchain_barrier.srcAccessMask = 0;
    swapchain_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &swapchain_barrier);

    // Copy from draw_image_ to swapchain image
    VkImageCopy copy_region = {};
    copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.srcSubresource.baseArrayLayer = 0;
    copy_region.srcSubresource.layerCount = 1;
    copy_region.srcSubresource.mipLevel = 0;
    copy_region.srcOffset = {.x=0, .y=0, .z=0};
    copy_region.dstSubresource = copy_region.srcSubresource;
    copy_region.dstOffset = {.x=0, .y=0, .z=0};
    copy_region.extent = {.width=window_extent_.width, .height=window_extent_.height, .depth=1};

    vkCmdCopyImage(cmd, draw_image_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchain_images_[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &copy_region);

    // Transition swapchain image for presentation
    swapchain_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchain_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    swapchain_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapchain_barrier.dstAccessMask = 0;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &swapchain_barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.pWaitDstStageMask = &wait_stage;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &get_current_frame().swapchain_semaphore;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &get_current_frame().render_semaphore;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    vkQueueSubmit(graphics_queue_, 1, &submit, get_current_frame().render_fence);

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pSwapchains = &swapchain_;
    present_info.swapchainCount = 1;
    present_info.pWaitSemaphores = &get_current_frame().render_semaphore;
    present_info.waitSemaphoreCount = 1;
    present_info.pImageIndices = &swapchain_image_index;

    vkQueuePresentKHR(graphics_queue_, &present_info);

    frame_number_++;
}


RenderServer::~RenderServer() {
	PRINT(bold | fg(blue), "Begun cleaning up RenderServer\n");

	SDL_HideWindow(window_);

	vkDeviceWaitIdle(device_);

	for (VkImage image : deletion_lists_.images) {
		vkDestroyImage(device_, image, nullptr);
	}
	deletion_lists_.images.clear();

	for (VkBuffer buffer : deletion_lists_.buffers) {
		vkDestroyBuffer(device_, buffer, nullptr);
	}
	deletion_lists_.buffers.clear();

	for (VkImageView view : deletion_lists_.image_views) {
		vkDestroyImageView(device_, view, nullptr);
	}
	deletion_lists_.image_views.clear();

	for (VkFramebuffer framebuffer : deletion_lists_.framebuffers) {
		vkDestroyFramebuffer(device_, framebuffer, nullptr);
	}
	deletion_lists_.framebuffers.clear();

	for (VkCommandPool pool : deletion_lists_.command_pools) {
		vkDestroyCommandPool(device_, pool, nullptr);
	}
	deletion_lists_.command_pools.clear();

	for (VkFence fence : deletion_lists_.fences) {
		vkDestroyFence(device_, fence, nullptr);
	}
	deletion_lists_.fences.clear();

	for (VkSemaphore semaphore : deletion_lists_.semaphores) {
		vkDestroySemaphore(device_, semaphore, nullptr);
	}
	deletion_lists_.semaphores.clear();

	for (VkImage image : deletion_lists_.images) {
		vkDestroyImage(device_, image, nullptr);
	}
	deletion_lists_.images.clear();

	for (VkBuffer buffer : deletion_lists_.buffers) {
		vkDestroyBuffer(device_, buffer, nullptr);
	}
	deletion_lists_.buffers.clear();

	for (VkImageView view : deletion_lists_.image_views) {
		vkDestroyImageView(device_, view, nullptr);
	}
	deletion_lists_.image_views.clear();

	for (VkFramebuffer framebuffer : deletion_lists_.framebuffers) {
		vkDestroyFramebuffer(device_, framebuffer, nullptr);
	}
	deletion_lists_.framebuffers.clear();

	for (VkSwapchainKHR swapchain : deletion_lists_.swapchains) {
		vkDestroySwapchainKHR(device_, swapchain, nullptr);
	}
	deletion_lists_.swapchains.clear();

	for (VkCommandPool pool : deletion_lists_.command_pools) {
		vkDestroyCommandPool(device_, pool, nullptr);
	}
	deletion_lists_.command_pools.clear();

	for (VkFence fence : deletion_lists_.fences) {
		vkDestroyFence(device_, fence, nullptr);
	}
	deletion_lists_.fences.clear();

	for (VkRenderPass render_pass : deletion_lists_.render_passes) {
		vkDestroyRenderPass(device_, render_pass, nullptr);
	}
	deletion_lists_.render_passes.clear();

	vkDestroySurfaceKHR(instance_, surface_, nullptr);

	vkb::destroy_debug_utils_messenger(instance_, debug_messenger_);
	vkDestroyDevice(device_, nullptr);
	vkDestroyInstance(instance_, nullptr);

	vmaDestroyAllocator(allocator_);
	
	SDL_DestroyWindow(window_);
	SDL_Quit();

	PRINT(bold | fg(blue), "Finished cleaning up RenderServer\n\n");
};
