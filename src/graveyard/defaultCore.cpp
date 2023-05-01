//
// Created by darby on 1/27/2023.
//

#include <iostream>
#include <assert.h>
#include "defaultCore.hpp"


namespace puffin {

    VkResult defaultCore::createWindow(int width, int height, GLFWframebuffersizefun frameBufferResizeCallback) {
        usingDefaultWindow = true;

        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        defaultWindow = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(defaultWindow, this);

        if(frameBufferResizeCallback) {
            glfwSetFramebufferSizeCallback(defaultWindow, frameBufferResizeCallback);
        }

        return VK_SUCCESS;
    }

    void defaultCore::cleanupWindow(){
        assert(usingDefaultWindow);
        glfwDestroyWindow(defaultWindow);
        glfwTerminate();
    }

    void defaultCore::createInstanceWithValidationLayers(bool usingGLFW, std::vector<const char*> validationLayers) {
        usingDefaultInstance = true;

        // Enable validation layers
        if(!puffin::tools::checkValidationLayerSupport(validationLayers)) {
            throw std::runtime_error("Validation layers not supported!");
        }

        VkApplicationInfo appInfo {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "App Name";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        auto extensions = puffin::tools::getAllRequiredExtensions(usingGLFW, true);

        VkInstanceCreateInfo instanceCreateInfo {};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> mExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, mExtensions.data());

        // print out extension support
        std::cout << "___Extensions Available on this Device___" << std::endl;
        for(const auto& extension : mExtensions) {
            std::cout << '\t' << extension.extensionName << '\n';
        }

        if(usingGLFW && !puffin::tools::checkGLFWRequiredExtensionsSupport(mExtensions)) {
            throw std::runtime_error("Using GLFW without required extension support");
        }

        instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
        populateDebugMessengerCreateInfo(debugCreateInfo);

        if(vkCreateInstance(&instanceCreateInfo, nullptr, &defaultInstance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create default-core instance");
        }
    }

    void defaultCore::cleanupInstance() {
        assert(usingDefaultInstance);
        vkDestroyInstance(defaultInstance, nullptr);
    }

    VkResult defaultCore::createDebugUtilsMessengerEXT(VkInstance instance) {
        usingDefaultDebugMessenger = true;

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        populateDebugMessengerCreateInfo(createInfo);

        if(func != nullptr) {
            return func(instance, &createInfo, nullptr, &defaultDebugMessenger);
        } else {
            std::cout << "ext not present" << std::endl;
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void defaultCore::cleanupDebugMessager(VkInstance instance) {
        assert(usingDefaultDebugMessenger);
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

        if(func != nullptr) {
            func(instance, defaultDebugMessenger, nullptr);
        }
    }

    void defaultCore::createSurfaceViaGLFW(VkInstance instance, GLFWwindow *window) {
        usingDefaultSurface = true;

        if(glfwCreateWindowSurface(instance, window, nullptr, &defaultSurface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create a default window surface!");
        }
    }

    void defaultCore::cleanupSurface(VkInstance instance) {
        assert(usingDefaultSurface);

        vkDestroySurfaceKHR(instance, defaultSurface, nullptr);
    }

    void defaultCore::createVulkanDeviceWithValidationLayers(VkInstance instance, VkSurfaceKHR surface, VkQueueFlags requestedQueueTypes, std::vector<const char*> validationLayers) {

        // Pick our physical device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if(deviceCount == 0){
            throw std::runtime_error("failed to find GPUs with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        std::vector<puffin::tools::VulkanPhysicalDeviceSettings> settingsList {};

        puffin::tools::VulkanPhysicalDeviceSettings primarySettings{};
        primarySettings.deviceExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        primarySettings.queueFlagsSupported = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        primarySettings.supportsGeometryShader = true;
        primarySettings.supportsSamplerAnisotropy = true;
        primarySettings.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        settingsList.push_back(primarySettings);

        puffin::tools::VulkanPhysicalDeviceSettings secondarySettings{};
        secondarySettings.deviceExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        secondarySettings.queueFlagsSupported = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        secondarySettings.supportsGeometryShader = true;
        secondarySettings.supportsSamplerAnisotropy = true;
        secondarySettings.deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
        settingsList.push_back(secondarySettings);

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        for(auto& settings : settingsList) {
            physicalDevice = puffin::tools::findAppropriatePhysicalDevice(devices, surface, settings);

            if(physicalDevice != VK_NULL_HANDLE) {
                std::cout << "adequate device found" << std::endl;
                break;
            }
        }

        if(physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU");
        }

        defaultVulkanDevice = new puffin::VulkanDevice(physicalDevice);

        VkPhysicalDeviceFeatures deviceFeatures {};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        std::vector<const char*> enabledExtensions {};

        std::vector<const char*> enabledLayers {};

        for(auto layerName : validationLayers) {
            enabledLayers.push_back(layerName);
        }



        defaultVulkanDevice -> createLogicalDevice(
                deviceFeatures,
                enabledExtensions,
                enabledLayers,
                nullptr,
                true,
                requestedQueueTypes
        );

        /*vkGetDeviceQueue(defaultVulkanDevice->logicalDevice, defaultVulkanDevice->queueFamilyIndices.graphics, 0, &graphicsQueue);
        vkGetDeviceQueue(defaultVulkanDevice->logicalDevice, defaultVulkanDevice->queueFamilyIndices.transfer, 0, &transferQueue);
        vkGetDeviceQueue(defaultVulkanDevice->logicalDevice, defaultVulkanDevice->getPresentQueueFamilyIndex(surface), 0, &presentQueue);*/
    }

    void defaultCore::cleanupVulkanDevice() {
        assert(usingDefaultVulkanDevice);
        delete defaultVulkanDevice;
    }

    void defaultCore::createVulkanSwapchain(VkInstance instance, VulkanDevice *device, VkSurfaceKHR surface, uint32_t width, uint32_t height) {
        usingDefaultSwapchain = true;

        defaultSwapchain.connect(instance, device->physicalDevice, device->logicalDevice, surface);
        defaultSwapchain.create(&width, &height);

        std::cout << "defaultSwapchain created with height: " << height << " and width: " << width << std::endl;
    }

    void defaultCore::cleanupVulkanSwapchain() {
        assert(usingDefaultSwapchain);
        defaultSwapchain.cleanup();
    }

    void defaultCore::createImguiGLFWAndVulkanContext(GLFWwindow *window, VkRenderPass imguiRenderPass, ImGui_ImplVulkan_InitInfo initInfo){

        IMGUI_CHECKVERSION(); // WAIT MAYBE IMGUI CONTEXT SHOULD BE IN CORE FOR MORE OBVIOUS CLEANUP
        ImGui::CreateContext();

        // When & is placed in front of a name during a variable declaration, that
        // means that a "reference to" is being declared. ra's declarations reads
        // that ra is a reference to an integer. As such, it can only be assigned an
        // l-value, in this case a. For example...
        ImGuiIO& io = ImGui::GetIO(); (void)io;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(window, true);
        ImGui_ImplVulkan_Init(&initInfo, imguiRenderPass);

        io.Fonts->AddFontDefault();
    }

    void defaultCore::uploadImguiFont(VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkDevice device, VkQueue graphicsQueue) {
        VUB_CHECK_RESULT(vkResetCommandPool(device, commandPool, VK_FLAGS_NONE));
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VUB_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);

        VUB_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo endInfo = {};
        endInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        endInfo.commandBufferCount = 1;
        endInfo.pCommandBuffers = &commandBuffer;
        VUB_CHECK_RESULT(vkQueueSubmit(graphicsQueue, 1, &endInfo, VK_NULL_HANDLE));

        VUB_CHECK_RESULT(vkDeviceWaitIdle(device));

        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void defaultCore::cleanupImguiGLFWAndVulkanContext() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }


    // PRIVATE
    void defaultCore::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = puffin::tools::debugCallback;
        createInfo.pUserData = nullptr; // Optional
    }

}

