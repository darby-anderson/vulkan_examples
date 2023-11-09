//
// Created by darby on 1/27/2023.
//

#pragma once

#include <stdexcept>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include <vulkan/vulkan.h>
#include "GLFW/glfw3.h"


#include "vulkan_device.hpp"
#include "VulkanSwapchain.hpp"

namespace puffin {

const std::vector<const char*> defaultValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
};

struct defaultCore {

public:
    // WINDOW
    bool usingDefaultWindow;
    GLFWwindow* defaultWindow; // platform-agnostic defaultWindow
    VkResult createWindow(int width, int height, GLFWframebuffersizefun frameBufferResizeCallback = nullptr);
    void cleanupWindow();

    // INSTANCE
    bool usingDefaultInstance;
    VkInstance defaultInstance;
    void createInstanceWithValidationLayers(bool usingGLFW, std::vector<const char*> validationLayers = defaultValidationLayers);
    void cleanupInstance();

    // DEBUG MESSENGER
    bool usingDefaultDebugMessenger;
    VkDebugUtilsMessengerEXT defaultDebugMessenger;
    VkResult createDebugUtilsMessengerEXT(VkInstance instance);
    void cleanupDebugMessager(VkInstance instance);

    // SURFACE
    bool usingDefaultSurface;
    VkSurfaceKHR defaultSurface;
    void createSurfaceViaGLFW(VkInstance instance, GLFWwindow* window);
    void cleanupSurface(VkInstance instance);

    // DEVICE
    bool usingDefaultVulkanDevice;
    puffin::VulkanDevice *defaultVulkanDevice;
    void createVulkanDeviceWithValidationLayers(VkInstance instance, VkSurfaceKHR surface, VkQueueFlags requestedQueueTypes, std::vector<const char*> validationLayers = defaultValidationLayers);
    void cleanupVulkanDevice();

    // SWAPCHAIN
    bool usingDefaultSwapchain;
    puffin::VulkanSwapchain defaultSwapchain;
    void createVulkanSwapchain(VkInstance instance, VulkanDevice *device, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void cleanupVulkanSwapchain();

    // IMGUI
    bool usingDefaultImguiContext;
    ImGuiIO io;
    void createImguiGLFWAndVulkanContext(GLFWwindow *window, VkRenderPass imguiRenderPass, ImGui_ImplVulkan_InitInfo initInfo);
    void uploadImguiFont(VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkDevice device, VkQueue graphicsQueue);
    void cleanupImguiGLFWAndVulkanContext();

private:
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

};

} // vub



