/**
 * Partially based off of Sascha Willem's implmentation: https://github.com/SaschaWillems/Vulkan/
 * 
*/

#pragma once

#include <vector>

#include <vulkan/vulkan.h>
// #include <stb_image.h>
#include "stb_image.h"


#include "VulkanBuffer.hpp"
#include "vulkan_device.hpp"
#include "VulkanTools.hpp"
#include "VulkanInitializers.hpp"

namespace puffin {

class Texture
{
	public:
		puffin::VulkanDevice* 		device;
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
        // KTX is a texture format for

};

class Texture2D : public Texture 
{
	public:
		void loadFromFile(
                std::string			filename,
                VkFormat			format,
                puffin::VulkanDevice *	device,
                VkQueue				copyQueue,
                uint32_t            mipLevels,
                VkImageUsageFlags	imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
                VkImageLayout		imageLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		void fromBuffer(
                void *				buffer,
                VkDeviceSize		bufferSize,
                VkFormat			format,
                uint32_t			texWidth,
                uint32_t			texHeight,
                puffin::VulkanDevice * device,
                VkQueue				copyQueue,
                VkFilter			filter			= VK_FILTER_LINEAR,
                VkImageUsageFlags	imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
                VkImageLayout		imageLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
};

}