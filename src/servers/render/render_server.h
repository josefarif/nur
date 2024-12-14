#pragma once

#define VMA_VULKAN_VERSION 1000000
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <vector>

#include "nur.h"

struct FrameData {
		VkCommandPool command_pool;
		VkCommandBuffer main_command_buffer;
		VkFence render_fence;
		VkSemaphore swapchain_semaphore, render_semaphore;
};

constexpr unsigned int kFrameOverlap = 2;

struct DeletionLists {
		std::vector<VkImage> images;
		std::vector<VkBuffer> buffers;
		std::vector<VkImageView> image_views;
		std::vector<VkFramebuffer> framebuffers;
		std::vector<VkCommandPool> command_pools;
		std::vector<VkFence> fences;
		std::vector<VkSemaphore> semaphores;
		std::vector<VkSwapchainKHR> swapchains;
		std::vector<VkRenderPass> render_passes;
};

struct AllocatedImage {
		VkImage image;
		VkImageView imageView;
		VmaAllocation allocation;
		VkExtent3D imageExtent;
		VkFormat imageFormat;
};

class RenderServer {
	public:
		RenderServer();

		~RenderServer();

		RenderServer(const RenderServer &) = delete;
		auto operator=(const RenderServer &) -> RenderServer & = delete;
		RenderServer(RenderServer &&) = delete;
		auto operator=(RenderServer &&) -> RenderServer & = delete;

		void draw();

	private:
		SDL_Window *window_;
		VkInstance instance_;
		VkDebugUtilsMessengerEXT debug_messenger_;
		VkSurfaceKHR surface_;
		VkPhysicalDevice physical_device_;
		VkDevice device_;

		VkQueue graphics_queue_;
		uint32_t graphics_queue_family_;

		VkExtent2D window_extent_{1280, 720};

		VkSwapchainKHR swapchain_;
		VkFormat swapchain_image_format_;
		std::vector<VkImage> swapchain_images_;
		std::vector<VkImageView> swapchain_image_views_;

		VkRenderPass render_pass_;
		std::vector<VkFramebuffer> framebuffers_;

		uint64_t frame_number_{0};

		FrameData frames_[kFrameOverlap];
		FrameData &get_current_frame() { return frames_[frame_number_ % kFrameOverlap]; };

		AllocatedImage draw_image_;
		VkFramebuffer draw_image_framebuffer_;
		VkExtent2D draw_extent_;

		VmaAllocator allocator_;

		DeletionLists deletion_lists_;
};