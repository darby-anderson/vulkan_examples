#include "VulkanTools.hpp"

namespace vub {
namespace tools {
    
    std::string errorString(VkResult errorCode) {

        switch(errorCode) {
        
#define STR(r) case VK_ ##r: return #r
                STR(NOT_READY);       
                STR(TIMEOUT);       
                STR(EVENT_SET);       
                STR(EVENT_RESET);       
                STR(INCOMPLETE);       
                STR(ERROR_OUT_OF_HOST_MEMORY);       
                STR(ERROR_OUT_OF_DEVICE_MEMORY);       
                STR(ERROR_INITIALIZATION_FAILED);       
                STR(ERROR_DEVICE_LOST);       
                STR(ERROR_MEMORY_MAP_FAILED);       
                STR(ERROR_LAYER_NOT_PRESENT);
				STR(ERROR_EXTENSION_NOT_PRESENT);
				STR(ERROR_FEATURE_NOT_PRESENT);
				STR(ERROR_INCOMPATIBLE_DRIVER);
				STR(ERROR_TOO_MANY_OBJECTS);
				STR(ERROR_FORMAT_NOT_SUPPORTED);
				STR(ERROR_SURFACE_LOST_KHR);
				STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
				STR(SUBOPTIMAL_KHR);
				STR(ERROR_OUT_OF_DATE_KHR);
				STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
				STR(ERROR_VALIDATION_FAILED_EXT);
				STR(ERROR_INVALID_SHADER_NV);   
#undef STR
            default:
                return "UNKNOWN_ERROR";
        }

    }
   
    VkPhysicalDevice findAppropriatePhysicalDevice(std::vector<VkPhysicalDevice> devices, VkSurfaceKHR surface, VulkanPhysicalDeviceSettings settings){
        
        for(VkPhysicalDevice device : devices) {

            // Check features
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);

            if(settings.deviceType != deviceProperties.deviceType) {
                std::cout << "Device does not support requested device type" << std::endl;
                continue;
            }

            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            if(settings.supportsGeometryShader && !deviceFeatures.geometryShader) {
                std::cout << "Device does not support requested geometry shader" << std::endl;
                continue;
            }

            if(settings.supportsSamplerAnisotropy && !deviceFeatures.samplerAnisotropy) {
                std::cout << "Device does not support requested sampler anisotropy" << std::endl;
                continue;
            }

            // Check the queue families
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            bool queueFamilyMatchFound = false;
            for(const auto& queueFamily : queueFamilies) {
                if((queueFamily.queueFlags & settings.queueFlagsSupported) == settings.queueFlagsSupported) {
                    queueFamilyMatchFound = true;
                    break;
                }
            }

            if(!queueFamilyMatchFound) {
                std::cout << "Device does not support requested queue family" << std::endl;
                continue;
            }

            // Extension Support
            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

            std::set<std::string> requiredExtensions (settings.deviceExtensions.begin(), settings.deviceExtensions.end());

            for(const auto& extension : availableExtensions) {
                requiredExtensions.erase(extension.extensionName);
            }

            if(!requiredExtensions.empty()) {
                std::cout << "Missing requested extensions on device: " << std::endl;
                for(std::string extensionName : requiredExtensions) {
                    std::cout << extensionName << std::endl;
                }
                continue;
            }

            // Swapchain Support
            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

            if(formatCount == 0) {
                std::cout << "Physical device supports no surface formats" << std::endl;
                continue;
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

            if(presentModeCount == 0){
                std::cout << "Physical device supports no present modes" << std::endl;
                continue;
            }

            return device;
        }

        return VK_NULL_HANDLE;
    }

    /**
     * @brief Create an image memory barrier for changing the layout of an image
     *  and put it into an active command buffer.
     *  
     * 
     * @param cmdBuffer the buffer in which to place the barrier
     * @param image the image which is having its layout changed
     * @param oldImageLayout the current image layout of the image
     * @param newImageLayout the image layout to switch to 
     * @param subresourceRange the image subresource range within the image to be affected by the barrier
     * @param srcStageMask the first synchronization/access scope -> the image operations in this stage must occur before those specified in dstStageMask 
     * @param dstStageMask the second synchronization/access scope -> the image operations must wait until srcStageMask's operations are complete 
     */
    void setImageLayout(
        VkCommandBuffer cmdBuffer,
        VkImage image,
        VkImageLayout oldImageLayout,
        VkImageLayout newImageLayout,
        VkImageSubresourceRange subresourceRange,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask
    ) 
    {
        // Create an image barrier object
        VkImageMemoryBarrier imageMemoryBarrier = vub::initializers::imageMemoryBarrier();
        imageMemoryBarrier.oldLayout = oldImageLayout;
        imageMemoryBarrier.newLayout = newImageLayout;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;

        // Source layouts (old)
        // Source access mask controls actions that have to be finished on the old layout
        // before it will be transitioned to the new layout
        switch(oldImageLayout){
            case VK_IMAGE_LAYOUT_UNDEFINED:
                // Image layout is undefined (or doesn't matter)
                // Only valid as initial layout
                // No flags required, listed only for completeness
                imageMemoryBarrier.srcAccessMask = 0;
                break;

            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                // Image is preinitialized
                // Only valid as initial layout for linear images, preserves memory contents
                // Make sure host writes have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image is a color attachment
                // Make sure any writes to the depth/stencil buffer have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image is a color attachment
                // Make sure any writes to the depth/stencil buffer have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image is a transfer source
				// Make sure any reads from the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image is a transfer destination
				// Make sure any writes to the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image is a transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;

            default:
                break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch(newImageLayout)
        {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image will be used as a transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;
            
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image will be used as a transfer source
                // Make sure any reads from the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image will be used as a color attachment
                // Make sure any writes to the color buffer have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image layout will be used as a depth/stencil attachment
                // Make sure any writes to depth/stencil buffer have been finished
                imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image will be read in a shader (sampler, input attachment)
                // Make sure any writes to the image have been finished
                if(imageMemoryBarrier.srcAccessMask == 0) {
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                }
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            
            default:
                break;
        }

        // Put barrier in setup command buffer
        vkCmdPipelineBarrier(
            cmdBuffer,
            srcStageMask,
            dstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier
        );

    }

    void setImageLayout(
        VkCommandBuffer cmdBuffer,
        VkImage image,
        VkImageAspectFlags aspectMask,
        VkImageLayout oldImageLayout,
        VkImageLayout newImageLayout,
        VkImageSubresourceRange subresourceRange,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask
    )
    {
        VkImageSubresourceRange subresouceRanage = {};
        subresouceRanage.aspectMask = aspectMask;
        subresouceRanage.baseMipLevel = 0;
        subresouceRanage.levelCount = 1;
        subresouceRanage.layerCount = 1;
        setImageLayout(cmdBuffer, image, oldImageLayout, newImageLayout, subresouceRanage, srcStageMask, dstStageMask);
    }


}
}