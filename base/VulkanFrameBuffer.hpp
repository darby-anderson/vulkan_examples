//
// Created by darby on 1/25/2023.
//

#pragma once

#include <algorithm>
#include <iterator>
#include <vector>
#include "vulkan/vulkan.h"
#include "VulkanDevice.hpp"
#include "VulkanTools.hpp"


namespace vub {

/*
 * -- Render passes
 * Draw commands must be recorded within a render pass instance.
 * Each render pass instance defines a set of image resources, referred to as attachments, used during rendering.
 *
 * -- Framebuffers
 * Render passes operate in conjunction with framebuffers.
 * Framebuffers represent a collection of specific memory attachments that a render pass instance uses.
 */

/**
 * @brief Encapsulates a single frame buffer attachment
 */
struct FramebufferAttachment {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkFormat format;
    VkImageSubresourceRange subresourceRange;
    VkAttachmentDescription description;

    /**
     * @return true if the attachment has a depth component
     */
    bool hasDepth() {
        std::vector<VkFormat> formats = {
                VK_FORMAT_D16_UNORM,
                VK_FORMAT_X8_D24_UNORM_PACK32,
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
        };
        return std::find(formats.begin(), formats.end(), format) != std::end(formats);
    }

    /**
     * @return true if the attachment has a stencil component
     */
     bool hasStencil() {
         std::vector<VkFormat> formats = {
                 VK_FORMAT_S8_UINT,
                 VK_FORMAT_D16_UNORM_S8_UINT,
                 VK_FORMAT_D24_UNORM_S8_UINT,
                 VK_FORMAT_D32_SFLOAT_S8_UINT
         };
         return std::find(formats.begin(), formats.end(), format) != std::end(formats);
     }

    /**
     * @return returns true if the attachment is a depth and/or stencil attachment
     */
     bool isDepthStencil()
    {
         return (hasDepth() || hasStencil());
    }

};

/**
 * @brief Describes the attributes of an attachment to be created
 */
struct AttachmentCreateInfo {
    uint32_t width, height;
    uint32_t layerCount;
    VkFormat format;
    VkImageUsageFlags usage;
    VkSampleCountFlagBits imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
};


/**
 * @brief Encapsulates a complete vulkan framebuffer with an arbitrary number and combination of attachments
 */
struct FrameBuffer {

private:
    vub::VulkanDevice *vulkanDevice;

public:
    uint32_t width, height;
    VkFramebuffer framebuffer;
    VkRenderPass renderPass;
    VkSampler sampler;
    std::vector<vub::FramebufferAttachment> attachments;

    /**
     * Default constructor
     *
     * @param vulkanDevice Pointer to a valid VulkanDevice
     */
    FrameBuffer(vub::VulkanDevice *vulkanDevice) {
        assert(vulkanDevice);
        this->vulkanDevice = vulkanDevice;
    }

    /**
     * Destroy and free Vulkan resources used for the framebuffer and all of its attachments
     */
     ~FrameBuffer() {
         assert(vulkanDevice);
         for(auto attachment : attachments) {
             vkDestroyImage(vulkanDevice->logicalDevice, attachment.image, nullptr);
             vkDestroyImageView(vulkanDevice->logicalDevice, attachment.view, nullptr);
             vkFreeMemory(vulkanDevice->logicalDevice, attachment.memory, nullptr);
         }
         vkDestroySampler(vulkanDevice->logicalDevice, sampler, nullptr);
         vkDestroyRenderPass(vulkanDevice->logicalDevice, renderPass, nullptr);
        vkDestroyFramebuffer(vulkanDevice->logicalDevice, framebuffer, nullptr);
     }

     /**
      * @brief Add a new attachment described by createinfo to the framebuffer's attachment list
      *
      * @param createInfo structure that specifies the framebuffer to be constructed
      * @return Index of the new attachment
      */
     uint32_t addAttachment(vub::AttachmentCreateInfo createInfo) {

         vub::FramebufferAttachment attachment;
         attachment.format = createInfo.format;

         VkImageAspectFlags aspectMask = VK_FLAGS_NONE;

         // Select the aspect mask and layout depending on usage

         // Color attachment
         if(createInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT){
             aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         }

         // Depth (and/or stencil) attachment
         if(createInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
             if(attachment.hasDepth()) {
                 aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
             }

             if(attachment.hasStencil()) {
                 aspectMask = aspectMask | VK_IMAGE_ASPECT_STENCIL_BIT;
             }
         }

         assert(aspectMask > 0);

         VkImageCreateInfo imageCreateInfo = vub::initializers::imageCreateInfo();
         imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
         imageCreateInfo.format = createInfo.format;
         imageCreateInfo.extent.width = createInfo.width;
         imageCreateInfo.extent.height = createInfo.height;
         imageCreateInfo.extent.depth = 1;
         imageCreateInfo.mipLevels = 1;
         imageCreateInfo.arrayLayers = createInfo.layerCount;
         imageCreateInfo.samples = createInfo.imageSampleCount;
         imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
         imageCreateInfo.usage = createInfo.usage;

         VkMemoryAllocateInfo memoryAllocateInfo = vub::initializers::memoryAllocateInfo();
         VkMemoryRequirements memoryRequirements;

         // Create the imageCreateInfo for this attachment
         VUB_CHECK_RESULT(vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &attachment.image));
         vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, attachment.image, &memoryRequirements);
         memoryAllocateInfo.allocationSize = memoryRequirements.size;
         memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
         VUB_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memoryAllocateInfo, nullptr, &attachment.memory));
         VUB_CHECK_RESULT(vkBindImageMemory(vulkanDevice->logicalDevice, attachment.image, attachment.memory, 0));

         attachment.subresourceRange = {};
         attachment.subresourceRange.aspectMask = aspectMask;
         attachment.subresourceRange.levelCount = 1;
         attachment.subresourceRange.layerCount = createInfo.layerCount;

         VkImageViewCreateInfo imageViewCreateInfo = vub::initializers::imageViewCreateInfo();
         imageViewCreateInfo.viewType = (createInfo.layerCount == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
         imageViewCreateInfo.format = createInfo.format;
         imageViewCreateInfo.subresourceRange = attachment.subresourceRange;
         imageViewCreateInfo.subresourceRange.aspectMask = (attachment.hasDepth()) ? VK_IMAGE_ASPECT_DEPTH_BIT : aspectMask;
         imageViewCreateInfo.image = attachment.image;
         VUB_CHECK_RESULT(vkCreateImageView(vulkanDevice->logicalDevice, &imageViewCreateInfo, nullptr, &attachment.view));

         // Fill attachment description
         attachment.description = {};
         attachment.description.samples = createInfo.imageSampleCount;
         attachment.description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
         attachment.description.storeOp = (createInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
         attachment.description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
         attachment.description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
         attachment.description.format = createInfo.format;
         attachment.description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

         // Final layout
         // If not, final layout depends on attachment type
         if(attachment.hasDepth() || attachment.hasStencil()) {
             attachment.description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
         } else {
             attachment.description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
         }

         attachments.push_back(attachment);

         return static_cast<uint32_t>(attachments.size() - 1);
     }


     /**
      * @brief Creates a default sampler for sampling from any of the framebuffer attachments
      * Applications are free to create their own samplers for different use cases
      *
      * @param magnificationFilter Magnification filter for lookups
      * @param minificationFilter Minification filter for lookups
      * @param addressMode Addressing mode for the U,V and W coordinates
      * @return VkResult for the sampler creation
      */
     VkResult createSampler(VkFilter magnificationFilter, VkFilter minificationFilter, VkSamplerAddressMode addressMode) {
         VkSamplerCreateInfo samplerInfo = vub::initializers::samplerCreateInfo();
         samplerInfo.magFilter = magnificationFilter;
         samplerInfo.minFilter = minificationFilter;
         samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
         samplerInfo.addressModeU = addressMode;
         samplerInfo.addressModeV = addressMode;
         samplerInfo.addressModeW = addressMode;
         samplerInfo.mipLodBias = 0.0f;
         samplerInfo.maxAnisotropy = 1.0f;
         samplerInfo.minLod = 0.0f;
         samplerInfo.maxLod = 1.0f;
         samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
         return vkCreateSampler(vulkanDevice->logicalDevice, &samplerInfo, nullptr, &sampler);
     }

     /**
      *  @brief Creates a default render pass setup with one sub pass
      *
      * @return VK_SUCCESS if all resources have been created successfully
      */
     VkResult createRenderPass() {

         std::vector<VkAttachmentDescription> attachmentDescriptions;
         for(auto& attachment : attachments) {
             attachmentDescriptions.push_back(attachment.description);
         }

         // Collect the attachment references
         std::vector<VkAttachmentReference> colorReferences;
         VkAttachmentReference depthReference = {};
         bool hasDepth = false;
         bool hasColor = false;

         uint32_t attachmentIndex = 0;

         for(auto& attachment : attachments) {
             if(attachment.isDepthStencil()) {
                 // Only one depth attachment allowed
                 assert(!hasDepth);
                 depthReference.attachment = attachmentIndex;
                 depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                 hasDepth = true;
             } else {
                 VkAttachmentReference colorReference;
                 colorReference.attachment = attachmentIndex;
                 colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                 colorReferences.push_back(colorReference);
                 hasColor = true;
             }
            attachmentIndex++;
         }

         // Default render pass setup uses only one subpass
         VkSubpassDescription subpass = {};
         subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
         if(hasColor) {
             subpass.pColorAttachments = colorReferences.data();
             subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
         }
         if(hasDepth) {
             subpass.pDepthStencilAttachment = &depthReference;
         }

         // Use subpass dependencies for attachment layout transitions
         std::array<VkSubpassDependency, 2> dependencies;

         dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
         dependencies[0].dstSubpass = 0;
         dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
         dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
         dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
         dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
         dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

         dependencies[1].srcSubpass = 0;
         dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
         dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
         dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
         dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
         dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
         dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

         // Create render pass
         VkRenderPassCreateInfo renderPassInfo = {};
         renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
         renderPassInfo.pAttachments = attachmentDescriptions.data();
         renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
         renderPassInfo.pSubpasses = &subpass;
         renderPassInfo.subpassCount = 1;
         renderPassInfo.pDependencies = dependencies.data();
         renderPassInfo.dependencyCount = 2;
         VUB_CHECK_RESULT(vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &renderPass));

         std::vector<VkImageView> attachmentViews;
         for(auto attachment : attachments) {
             attachmentViews.push_back(attachment.view);
         }

         // Find max number of layers across attachments
         uint32_t maxLayers = 0;
         for(auto attachment : attachments) {
             if(attachment.subresourceRange.layerCount > maxLayers) {
                 maxLayers = attachment.subresourceRange.layerCount;
             }
         }

         VkFramebufferCreateInfo framebufferCreateInfo = {};
         framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
         framebufferCreateInfo.renderPass = renderPass;
         framebufferCreateInfo.pAttachments = attachmentViews.data();
         framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
         framebufferCreateInfo.width = width;
         framebufferCreateInfo.height = height;
         framebufferCreateInfo.layers = maxLayers;
         VUB_CHECK_RESULT(vkCreateFramebuffer(vulkanDevice->logicalDevice, &framebufferCreateInfo, nullptr, &framebuffer));

         return VK_SUCCESS;
     }

};



};
