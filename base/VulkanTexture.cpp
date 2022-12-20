#include "VulkanTexture.hpp"

namespace vub {


void Texture::updateDescriptor()
{
	descriptor.sampler = sampler;
	descriptor.imageView = view;
	descriptor.imageLayout = imageLayout;
}

void Texture::destroy()
{
	vkDestroyImageView(device->logicalDevice, view, nullptr);
	vkDestroyImage(device->logicalDevice, image, nullptr);
	if(sampler) 
	{
		vkDestroySampler(device->logicalDevice, sampler, nullptr);
	}
	vkFreeMemory(device->logicalDevice, deviceMemory, nullptr);
}

/**
 * @brief Creates a 2D texture from a buffer
 * 
 * @param buffer Buffer containing texture data to upload
 * @param bufferSize Size of the buffer in machine units
 * @param format Vulkan format of the image data stored in the file
 * @param texWidth Width of the texture to create
 * @param texHeight Height of the texture to create 
 * @param device Vulkan device to create the texture on
 * @param copyQueue Queue used for the texture staging copy commands (must support transfer)
 * @param (Optional) filter Texture filter for the sampler (defaults to VK_FILTER_LINEAR)
 * @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
 * @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
 */
void Texture2D::fromBuffer(
	void* buffer, 
	VkDeviceSize bufferSize, 
	VkFormat format, 
	uint32_t texWidth, 
	uint32_t texHeight, 
	vub::VulkanDevice* device, 
	VkQueue copyQueue,
	VkFilter filter,
	VkImageUsageFlags imageUsageFlags,
	VkImageLayout imageLayout	
)
{
	assert(buffer);

	this->device = device;
	width = texWidth;
	height = texHeight;
	mipLevels = 1;

	VkMemoryAllocateInfo memAllocInfo = vub::initializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	// Use a separate command buffer for texture loading
	VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Create a host-visible staging buffer taht contains the raw image data
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo = vub::initializers::bufferCreateInfo();
	bufferCreateInfo.size = bufferSize;

	// This buffer is used as a transfer source for the buffer copy
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VUB_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	// Get memory requirements for the staging buffer (alignment, memory type bits)
	vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;

	// Get Memory type index for a host visible buffer
	memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VUB_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
	VUB_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

	// Copy the texture data into staging buffer
	uint8_t *data;
	VUB_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	memcpy(data, buffer, bufferSize);
	vkUnmapMemory(device->logicalDevice, stagingMemory);

	VkBufferImageCopy bufferCopyRegion = {};
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.mipLevel = 0;
	bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = width;
	bufferCopyRegion.imageExtent.height = height;
	bufferCopyRegion.imageExtent.depth = 1;
	bufferCopyRegion.bufferOffset = 0;

	/// Create optimal tiled target image
	VkImageCreateInfo imageCreateInfo = vub::initializers::imageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.mipLevels = mipLevels;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.extent = { width, height, 1 };
	imageCreateInfo.usage = imageUsageFlags;

	// Ensure the TRANSFER_DST bit is set for staging
	if(!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) 
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	VUB_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

	vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;

	memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VUB_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
	VUB_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = mipLevels;
	subresourceRange.layerCount = 1;

	// Image barrier for optimal image
	// Optimal image will be used as destination for the copy
	vub::tools::setImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange
	);

	// Copy mip levels from staging buffer
	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&bufferCopyRegion
	);

	// Change texture image layout to  for the shader-read after all mip levels have been copied
	this->imageLayout = imageLayout;
	vub::tools::setImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		imageLayout,
		subresourceRange
	);

	// Submit my commands to the queue
	device->flushCommandBuffer(copyCmd, copyQueue);

	// Clean up the staging resources
	vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
	vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = filter;
	samplerCreateInfo.minFilter = filter;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	VUB_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

	// Create image view
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.pNext = NULL;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = format;
	viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	viewCreateInfo.subresourceRange =  { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.image = image;
	VUB_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

	// Update descriptor image info member that can be used for setting up descriptor sets
	updateDescriptor();

}

}