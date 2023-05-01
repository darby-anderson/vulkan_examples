//
// Created by darby on 1/19/2023.
//

#pragma once

#include <stdlib.h>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <exception>
#include <limits>

#include "vulkan/vulkan.h"
#include "GLFW/glfw3.h"
#include "VulkanTools.hpp"

namespace puffin {

typedef struct _SwapchainBuffers {
    VkImage image;
    VkImageView view;
} SwapchainBuffer ;


class VulkanSwapchain {

private:
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkSurfaceKHR surface;

public:
    VkFormat colorFormat;
    VkColorSpaceKHR colorSpace;
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    uint32_t imageCount;
    std::vector<VkImage> images;
    std::vector<SwapchainBuffer> buffers;
    uint32_t queueNodeIndex = UINT32_MAX;
    VkExtent2D extent2D;


    void connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface);
    void create(uint32_t* width, uint32_t* height, bool vsync = false, bool fullscreen = false);
    VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
    VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);
    void cleanup();

};

} // vub

