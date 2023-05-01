//
// Created by darby on 1/19/2023.
//

#include "VulkanSwapchain.hpp"

namespace puffin {

/**
 * Set instance, physical and logical device to use for the defaultSwapchain and get all required function pointers.
 * Also initializes the surface properties
 *
 * @param instance vulkan instance to use
 * @param physicalDevice physical device used to query properties and formats relevant to the defaultSwapchain
 * @param device logical representation fo the device to create the defaultSwapchain for
 * @param surface the surface to use in the defaultSwapchain
 */
void VulkanSwapchain::connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface) {
    this->instance = instance;
    this->physicalDevice = physicalDevice;
    this->device = device;
    this->surface = surface;

    // Get available queue family properties
    uint32_t queueCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);
    assert(queueCount >= 1);

    std::vector<VkQueueFamilyProperties> queueProps(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps.data());

    // Iterate over each queue to learn whether it supports presenting:
    // Find a queue with present support
    // Will be used to present the swap chain images to the windowing system
    std::vector<VkBool32> supportsPresent(queueCount);
    for(uint32_t i = 0; i < queueCount; i++) {
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportsPresent[i]);
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    uint32_t presentQueueNodeIndex = UINT32_MAX;
    for(uint32_t i = 0; i < queueCount; i++) {
        if((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            graphicsQueueNodeIndex = i;
        }

        if(supportsPresent[i] == VK_TRUE) {
            graphicsQueueNodeIndex = i;
            presentQueueNodeIndex = 0;
            break;
        }
    }

    if(presentQueueNodeIndex == UINT32_MAX) {
        // If there's no queue that supports both present and graphics, try to find a separate present queue
        for(uint32_t i = 0; i < queueCount; i++) {
            if(supportsPresent[i] == VK_TRUE) {
                presentQueueNodeIndex = i;
                break;
            }
        }
    }

    // Exit if either graphics or presenting queue hasn't been found
    if(graphicsQueueNodeIndex == UINT32_MAX)
    {
        throw new std::runtime_error("Could not find a graphics queue!");
    }

    if(presentQueueNodeIndex == UINT32_MAX) {
        throw new std::runtime_error("Could not find a presenting queue!");
    }

    if(graphicsQueueNodeIndex != presentQueueNodeIndex) {
        throw new std::runtime_error("Could not find a combined graphics and presenting queue");
    }

    queueNodeIndex = graphicsQueueNodeIndex;

    // Get list of supported surface formats
    uint32_t formatCount;
    VUB_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL));
    assert(formatCount > 0);

    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    VUB_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats.data()));

    // If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
    // there is no preferred format, so we assume VK_FORMAT_B8G8R8A8_UNORM
    if((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED)) {
        colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
        colorSpace = surfaceFormats[0].colorSpace;
    }
    else {
        // iterate over the list of available surface format and
        // check for the presence of VK_FORMAT_B8G8R8A8_UNORM
        bool found_B8G8R8A8_UNORM = false;
        for(auto&& surfaceFormat : surfaceFormats){
            if(surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM) {
                colorFormat = surfaceFormat.format;
                colorSpace = surfaceFormat.colorSpace;
                found_B8G8R8A8_UNORM = true;
                break;
            }
        }

        // in case VK_FORMAT_B8G8R8A8_UNORM is not available
        // select the first available color format
        if(!found_B8G8R8A8_UNORM) {
            colorFormat = surfaceFormats[0].format;
            colorSpace = surfaceFormats[0].colorSpace;
        }
    }
}

/**
 * Create the defaultSwapchain and get its images with given width and height
 *
 * @param width pointer to the width of the defaultSwapchain (may be adjusted to fit the requirements of the defaultSwapchain)
 * @param height pointer to the height of the defaultSwapchain (may be adjusted to fit the requirements of the defaultSwapchain)
 * @param vsync (Optional) can e used to force vsync-ed rendering (by using VK_PRESENT_MODE_FIFO_KHR as presentation mode)
 * @param fullScreen
 */
void VulkanSwapchain::create(uint32_t *width, uint32_t *height, bool vsync, bool fullScreen) {
    // Store the current swap chain handle, so we can use it later on to ease up re-creation
    VkSwapchainKHR oldSwapchain = swapChain;

    // Get the physical device surface properties and formats
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VUB_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));

    // Get available present modes
    uint32_t presentModeCount;
    VUB_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL));
    assert(presentModeCount > 0);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VUB_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()));

        extent2D  = {};
    // If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the defaultSwapchain
    if(surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
        // If the surface size is undefined, the size is set to
        // the size of the images requested
        extent2D = surfaceCapabilities.currentExtent;
        *width = surfaceCapabilities.currentExtent.width;
        *height = surfaceCapabilities.currentExtent.height;
    } else {
        extent2D.width = *width;
        extent2D.height = *height;
    }

    // Select a present mode for the defaultSwapchain

    // The VK_PRESENT_MODE_FIFO_KHR mode must always present as per spec
    // This mode waits for the vertical blank ("v-sync")
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    // If v-sync is not requested, try to find a mailbox mode
    // It's the lowest latency non-tearing present mode available
    if(!vsync) {
        for(size_t i = 0; i < presentModeCount; i++) {
            if(presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
            if(presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }

    // Determine the number of images
    uint32_t desiredNumberOfSwapchainImages = surfaceCapabilities.minImageCount + 1;

    if((surfaceCapabilities.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfaceCapabilities.maxImageCount)) {
        desiredNumberOfSwapchainImages = surfaceCapabilities.maxImageCount;
    }

    // Find the transformation of the surface
    VkSurfaceTransformFlagsKHR preTransform;
    if(surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        // We prefer a non-rotated transform
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else {
        preTransform = surfaceCapabilities.currentTransform;
    }

    // Find a supported composite alpha format (not all devices support alpha opaque)
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // Simply select the first composite alpha format available
    std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };

    for(auto& compositeAlphaFlag : compositeAlphaFlags) {
        if(surfaceCapabilities.supportedCompositeAlpha & compositeAlphaFlag) {
            compositeAlpha = compositeAlphaFlag;
            break;
        }
    }

    VkSwapchainCreateInfoKHR swapchainCI = {};
    swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCI.surface = surface;
    swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
    swapchainCI.imageFormat = colorFormat;
    swapchainCI.imageExtent = extent2D; // {extent2D.width, extent2D.height};
    swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
    swapchainCI.imageArrayLayers = 1;
    swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCI.queueFamilyIndexCount = 0;
    swapchainCI.presentMode = swapchainPresentMode;
    // Setting old defaultSwapchain to the saved handle of the previous defaultSwapchain aids in resource reuse and makes
    // sure that we can still present already acquired images
    swapchainCI.oldSwapchain = oldSwapchain;
    // Setting clipped to VK_TRUE allows the implementation to discard rendering outside the surface area
    swapchainCI.clipped = VK_TRUE;
    swapchainCI.compositeAlpha = compositeAlpha;

    // Enable transfer source on swap chain images if supported
    if(surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    // Enable transfer destination on swap chain images if supported
    if(surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    VUB_CHECK_RESULT(vkCreateSwapchainKHR(  device, &swapchainCI, nullptr, &swapChain));

    // If an existing defaultSwapchain is re-created, destroy the old defaultSwapchain
    // This also cleans up all the presentable images
    if(oldSwapchain != VK_NULL_HANDLE) {
        for(uint32_t i = 0; i < imageCount; i++) {
            vkDestroyImageView(device, buffers[i].view, nullptr);
        }
        vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
    }


    VUB_CHECK_RESULT(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, NULL));
    // Get the defaultSwapchain images
    images.resize(imageCount);
    VUB_CHECK_RESULT(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data()));

    // Get the swap chain buffers containing the image and imageview
    buffers.resize(imageCount);
    for(uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo colorAttachmentView = {};
        colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorAttachmentView.pNext = NULL;
        colorAttachmentView.format = colorFormat;
        colorAttachmentView.components = {
                VK_COMPONENT_SWIZZLE_R,
                VK_COMPONENT_SWIZZLE_G,
                VK_COMPONENT_SWIZZLE_B,
                VK_COMPONENT_SWIZZLE_A
        };
        colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentView.subresourceRange.baseMipLevel = 0;
        colorAttachmentView.subresourceRange.levelCount = 1;
        colorAttachmentView.subresourceRange.baseArrayLayer = 0;
        colorAttachmentView.subresourceRange.layerCount = 1;
        colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentView.flags = 0;

        buffers[i].image = images[i];
        colorAttachmentView.image = buffers[i].image;

        VUB_CHECK_RESULT(vkCreateImageView(device, &colorAttachmentView, nullptr, &buffers[i].view));
    }
}

/**
 * Acquires the next image in the swap chain
 *
 * @param presentCompleteSemaphore (Optional) Semaphore that is signaled when the image is
 *  ready for use
 * @param imageIndex pointer to the image index that will be increased if the next image could be acquired
 * @return VkResult of the image acquisition
 */
VkResult VulkanSwapchain::acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t *imageIndex) {
    // By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error
    // is thrown. Therefore, we don't have to handle VK_NOT_READY
    return vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, imageIndex);
}

/**
 * Queue an image for presentation
 *
 * @param queue Presentation queue for presenting an image
 * @param imageIndex Index of the defaultSwapchain image to queue for presentation
 * @param waitSemaphore (Optional) Semaphore that is waited on before the image is presented (only used if
 * != VK_NULL_HANDLE)
 *
 * @return VkResult of the queue presentation
 */
VkResult VulkanSwapchain::queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) {
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = NULL;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &imageIndex;

    // Check if a wait semaphore has been specified to wait for before presenting the image
    if(waitSemaphore != VK_NULL_HANDLE) {
        presentInfo.pWaitSemaphores = &waitSemaphore;
        presentInfo.waitSemaphoreCount = 1;
    }

    return vkQueuePresentKHR(queue, &presentInfo);
}

/**
 * Destroy and free Vulkan resources used for the defaultSwapchain
 */
void VulkanSwapchain::cleanup() {
    if(swapChain != VK_NULL_HANDLE) {
        for(uint32_t i = 0; i < imageCount; i++) {
            vkDestroyImageView(device, buffers[i].view, nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    /*if(surface != VK_NULL_HANDLE) { the swapchain isn't in charge of deleting the surface
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }*/

    surface = VK_NULL_HANDLE;
    swapChain = VK_NULL_HANDLE;
}

} // vub