/**
 * Partially based off of Sascha Willem's implmentation: https://github.com/SaschaWillems/Vulkan/
 * 
*/

#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include "VulkanBuffer.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTools.hpp"
#include "VulkanInitializers.hpp"

namespace vub {

class Texture
{
	public:
		vub::VulkanDevice* 		device;
		VkImage					image;
		VkImageLayout			imageLayout;
		VkDeviceMemory			deviceMemory;
		VkImageView				view;
		uint32_t				width, height;
		uint32_t				mipLevels;
		uint32_t				layerCount;
		VkDescriptorImageInfo	descriptor;
		VkSampler				sampler;

		void 	updateDescriptor();
		void 	destroy();
		// KTX SUPPORT??
};

class Texture2D : public Texture 
{
	public:
		/*void loadFromFile(
			std::string			filename,
			VkFormat			format,
			vub::VulkanDevice *	device,
			VkQueue				copyQueue,
			VkImageUsageFlags	imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout		imageLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			bool				forceLinear		= false
		);*/

		void fromBuffer(
			void *				buffer,
			VkDeviceSize		bufferSize,
			VkFormat			format,
			uint32_t			texWidth,
			uint32_t			texHeight,
			vub::VulkanDevice * device,
			VkQueue				copyQueue,
			VkFilter			filter			= VK_FILTER_LINEAR,
			VkImageUsageFlags	imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout		imageLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
};

}