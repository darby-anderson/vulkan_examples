#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imgui.h>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <set>
#include <cstring>
#include <optional>
#include <array>
#include <unordered_map>
#include <cstdint> // necessary for uint32_t
#include <limits> // necessary for std::numeric_limits
#include <algorithm> // necessary for std::clamp

// My Code
#include <camera.hpp>
#include <defaultCore.hpp>
#include "VulkanDevice.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanTools.hpp"
#include "VulkanTexture.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanFrameBuffer.hpp"
#include "vertex.hpp"
#include "model.hpp"
#include "material.hpp"

// This needs to go AFTER include VulkanTexture.hpp, since it also #includes <stb_image.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

//const std::string MODEL_PATH = "../../models/bell_x1-low_resolution-obj/bell_x1_mesh.obj";
const std::string MODEL_PATH = "../../models/uv_sphere.obj";

const std::string TEXTURE_PATH = "../../models/bell_x1-low_resolution-obj/bell_x1_diffuse.jpg";
const std::string VERTEX_SHADER_PATH = "../../shaders/vert.spv";
const std::string FRAGMENT_SHADER_PATH = "../../shaders/frag.spv";

const int MAX_FRAMES_IN_FLIGHT = 2;


#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif


/*
* This data is constant across a draw, and has only one instance in a draw 
*/
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};


/*
* VkSurfaceCapabilities describes basic surface capabilities (min/max defaultSwapchain images, min/max width and height of images)
* VkSurfaceFormat (pixel format)
* VkPresentMode - available presentation modes
*/
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};  

class HelloTriangleApplication {

public:

    void run(){
        core.createWindow(WIDTH, HEIGHT, frameBufferResizeCallback);

        initVulkan();

        ImGui_ImplVulkan_InitInfo initInfo = {};
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = core.defaultVulkanDevice->physicalDevice;
        initInfo.Device = core.defaultVulkanDevice->logicalDevice;
        initInfo.QueueFamily = core.defaultVulkanDevice->queueFamilyIndices.graphics;
        initInfo.Queue = graphicsQueue;
        initInfo.PipelineCache = VK_NULL_HANDLE;
        initInfo.DescriptorPool = imguiDescriptorPool;
        initInfo.Subpass = 0;
        initInfo.MinImageCount = core.defaultSwapchain.imageCount; // TODO: should I have this set to an actual min?
        initInfo.ImageCount = core.defaultSwapchain.imageCount;
        initInfo.MSAASamples = core.defaultVulkanDevice->getMaxUsableSampleCount();
        initInfo.Allocator = NULL;
        initInfo.CheckVkResultFn = nullptr;

        core.createImguiGLFWAndVulkanContext(core.defaultWindow, renderPass, initInfo);
        core.uploadImguiFont(graphicsCommandPool, commandBuffers[currentFrame], core.defaultVulkanDevice->logicalDevice, graphicsQueue);

        initObjects();
        mainLoop();
        cleanup();
    }

private:

    vub::defaultCore core;
    
    // Pointers to a memory allocation made for the index data

    // platform-agnostic defaultWindow
    // VkInstance inits the vulkan lib, and allows the app to pass info about itself to the implementation
    VkInstance instance;
    // A messenger object that handles passing along debug messages to a provided debug callback
    VkDebugUtilsMessengerEXT debugMessenger;
    // The logical device
    // Separates logical and physical devices. A physical device usually represents a single complete implementation of Vulkan available to the host
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue transferQueue;

    // Represents a collection of attachments, subpasses and dependencies between the subpasses, and describes how the attachments are used
    VkRenderPass renderPass;
    // An array of zero or more descriptor bindings.
    VkDescriptorSetLayout descriptorSetLayout;
    // Allows access to descriptor sets from a pipline
    VkPipelineLayout pipelineLayout;
    // Compute, ray tracing and graphics pipelines 
    VkPipeline graphicsPipeline;
    // framebuffer represent a collection of specific memory attachments that a render pass instance uses
    std::vector<VkFramebuffer> swapChainFramebuffers;
    // Opaque objects that command buffer memory is allocated from, which allows implementation to amortize the cost of resource creation across multiple command buffers
    VkCommandPool graphicsCommandPool;
    VkCommandPool transferCommandPool;
    // Records commands that can be subsequently submitted to a device queue for execution
    std::vector<VkCommandBuffer> commandBuffers;

    VkDescriptorPool descriptorPool;
    VkDescriptorPool imguiDescriptorPool;
    // A descriptor is a way for shaders to freely access resources like buffers and images
    std::vector<VkDescriptorSet> descriptorSets;

    // VkImage - Array of data that can be used for various purposes, in this case a texture
    // VkImageView - Represents contiguous range of the image subresources. used since image objects can't be directly used by the pipeline shaders
    // VkSampler - The object that describes how texels are sampled from texture images

    // Semaphores are sync. primitives that can be used to insert a dependency between queue operations or between a queue operation and the host
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    // sync primitive that can be used to insert a dependency from a queue to the host
    std::vector<VkFence> inFlightFences;

    // Tracks if the framebuffer was resized, as it is not guaranteed to be handled by the 
    bool framebufferResized = false;

    // Keeps track of the frame being rendered, vs the one being recorded
    uint32_t currentFrame = 0; 

    // My Objects
    vub::camera cam;

    vub::Texture2D texture;
    vub::Buffer vertexBuffer;
    vub::Buffer indexBuffer;
    std::vector<vub::Buffer> uniformBuffers;

    // vub::pctc_model sphereModel;
    vub::pc_model sphereModel;

    struct {
        VkImage image;    // Array of information, used for different purposes. This one is for storing depth buffer
        VkDeviceMemory mem;
        VkImageView view; // Represents contiguous range of the image sub-resources. used since image objects can't be directly used by the pipeline shaders. Contiguous for easy use by the shaders?
    } depthStencil;

    // Additional render target to place the multiple samples
    struct {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    } sampleRenderTarget;
    
    /*
    * Informs the app that a defaultWindow resize has occurred
    */
    static void frameBufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    void initVulkan() {

        core.createInstanceWithValidationLayers(true);
        core.createDebugUtilsMessengerEXT(core.defaultInstance);
        core.createSurfaceViaGLFW(core.defaultInstance, core.defaultWindow);
        core.createVulkanDeviceWithValidationLayers(core.defaultInstance, core.defaultSurface, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
        core.createVulkanSwapchain(core.defaultInstance, core.defaultVulkanDevice, core.defaultSurface, WIDTH, HEIGHT);

        // Get our device queues
        vkGetDeviceQueue(core.defaultVulkanDevice->logicalDevice, core.defaultVulkanDevice->queueFamilyIndices.graphics, 0, &graphicsQueue);
        vkGetDeviceQueue(core.defaultVulkanDevice->logicalDevice, core.defaultVulkanDevice->queueFamilyIndices.transfer, 0, &transferQueue);
        vkGetDeviceQueue(core.defaultVulkanDevice->logicalDevice, core.defaultVulkanDevice->getPresentQueueFamilyIndex(core.defaultSurface), 0, &presentQueue);

        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createCommandPools();
        createColorResources();
        createDepthResources();
        createFramebuffers();
        // texture.loadFromFile(TEXTURE_PATH, VK_FORMAT_R8G8B8A8_SRGB, core.defaultVulkanDevice, graphicsQueue, 2);
        sphereModel.loadObjFile(MODEL_PATH);
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createImguiDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }

    void initObjects() {
        // cam.lookAt(glm::vec3(1850.0f, 1850.0f, 1850.0f), glm::vec3(0.0f, 0.0f, 0.0f)); // airplane
        cam.lookAt(glm::vec3(3.0f, 3.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f));

        cam.setPerspective(45.0f, core.defaultSwapchain.extent2D.width / (float) core.defaultSwapchain.extent2D.height, 0.1f, 4000.0f);
    }

    void mainLoop() {
        while(!glfwWindowShouldClose(core.defaultWindow)){
            glfwPollEvents();

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            drawFrame();
        }

        vkDeviceWaitIdle(core.defaultVulkanDevice->logicalDevice);
    }


    void cleanup() {
        core.cleanupImguiGLFWAndVulkanContext();

        core.cleanupVulkanSwapchain();

        vkDestroyDescriptorPool(core.defaultVulkanDevice->logicalDevice, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(core.defaultVulkanDevice->logicalDevice, descriptorSetLayout, nullptr);

        vkDestroyDescriptorPool(core.defaultVulkanDevice->logicalDevice, imguiDescriptorPool, nullptr);


        vertexBuffer.destroy();
        indexBuffer.destroy();

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            uniformBuffers[i].destroy();
        }

        texture.destroy();

        vkDestroyImage(core.defaultVulkanDevice->logicalDevice, depthStencil.image, nullptr);
        vkDestroyImageView(core.defaultVulkanDevice->logicalDevice, depthStencil.view, nullptr);
        vkFreeMemory(core.defaultVulkanDevice->logicalDevice, depthStencil.mem, nullptr);

        vkDestroyImage(core.defaultVulkanDevice->logicalDevice, sampleRenderTarget.image, nullptr);
        vkDestroyImageView(core.defaultVulkanDevice->logicalDevice, sampleRenderTarget.view, nullptr);
        vkFreeMemory(core.defaultVulkanDevice->logicalDevice, sampleRenderTarget.mem, nullptr);

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(core.defaultVulkanDevice->logicalDevice, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(core.defaultVulkanDevice->logicalDevice, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(core.defaultVulkanDevice->logicalDevice, inFlightFences[i], nullptr);
        }
        
        vkDestroyCommandPool(core.defaultVulkanDevice->logicalDevice, graphicsCommandPool, nullptr);
        vkDestroyCommandPool(core.defaultVulkanDevice->logicalDevice, transferCommandPool, nullptr);

        vkDestroyPipelineLayout(core.defaultVulkanDevice->logicalDevice, pipelineLayout, nullptr);
        vkDestroyPipeline(core.defaultVulkanDevice->logicalDevice, graphicsPipeline, nullptr);

        vkDestroyRenderPass(core.defaultVulkanDevice->logicalDevice, renderPass, nullptr);

        for(auto& frameBuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(core.defaultVulkanDevice->logicalDevice, frameBuffer, nullptr);
        }

        core.cleanupVulkanDevice();

        if(enableValidationLayers) {
            core.cleanupDebugMessager(core.defaultInstance);
        }
        core.cleanupSurface(core.defaultInstance);

        core.cleanupWindow();
        core.cleanupInstance();
    }


    /*
    * Chooses format we like the best from the list of availableFormats
    */
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats){
        for(const auto& availableFormat : availableFormats){
            if(availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    /*
    * Chooses PRESENT_MODE_MAILBOX_KHR, if available
    */
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {

        for(const auto& availablePresentMode : availablePresentModes) {
            if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    /*
    * Uses information from glfw about the defaultWindow, to get the extent size we want
    */
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()){
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(core.defaultWindow, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }


    /*
    * This wraps the bytecode of a shader in a shader module, for entry into the pipeline
    */
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo {};
        createInfo.sType  = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if(vkCreateShaderModule(core.defaultVulkanDevice->logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS){
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    /*
    * Creates the descriptor sets passed into the vertex shader.
    */
    void createDescriptorSetLayout() {
        
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr; // optional
        
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerLayoutBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
        
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if(vkCreateDescriptorSetLayout(core.defaultVulkanDevice->logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    /*
    * Creates all stages of the graphics pipeline:
    *  1. Wraps and crates shader stages infos
    *  2. Describe the format of the vertex data being passed in 
    *  3. Describe the kind of geometry, and if primitive restart should be used
    *  4. Describe viewport and scissors
    *  5. Describe rasterization stage
    *  6. Describe the multi-sampling step
    *  7. Describe the color-blending step
    *    - Combining what is returned from the fragment shader. Either mixing old and new, or combining old and new bitwise
    *  8. Describe the dynamic state
    *    - What can be changed without re-creating the pipeline
    *  9. Define the pipeline layout
    *    - Formats uniform values
    *  10. Create the depth stencil description
    *     - Tests the depth of points, choosing the closest to draw
    *  11. Create the pipeline
    */
    void createGraphicsPipeline() {

        std::cout << "this is the current file path:" << std::endl;
        std::filesystem::path cwd = std::filesystem::current_path();
        std::cout << cwd.string() << std::endl;

        // 1.
        auto vertShaderCode = readFile(VERTEX_SHADER_PATH);
        auto fragShaderCode = readFile(FRAGMENT_SHADER_PATH);

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";
        
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";
        
        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // 2.
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        
        auto bindingDescription = vub::pctc_vertex::getBindingDescription();
        auto attributeDescriptions = vub::pctc_vertex::getAttributeDescriptions();
        
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription; 
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); 

        // 3.
        VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // 4.
        VkViewport viewport {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) core.defaultSwapchain.extent2D.width;
        viewport.height = (float) core.defaultSwapchain.extent2D.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = core.defaultSwapchain.extent2D;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        // 5.
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; //optional
        rasterizer.depthBiasClamp = 0.0f; //optional
        rasterizer.depthBiasSlopeFactor = 0.0f; //optional

        // 6.
        VkPipelineMultisampleStateCreateInfo multisampling {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = core.defaultVulkanDevice->getMaxUsableSampleCount();
        multisampling.minSampleShading = 1.0f; // optional
        multisampling.pSampleMask = nullptr; // optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // optional
        multisampling.alphaToOneEnable = VK_FALSE; // optional

        // 7.
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // optional 
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // optional 
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // optional 
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // optional 

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // optional
        colorBlending.attachmentCount = 1 ; 
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0; // optional
        colorBlending.blendConstants[1] = 0.0; // optional
        colorBlending.blendConstants[2] = 0.0; // optional
        colorBlending.blendConstants[3] = 0.0; // optional

        // 8.
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_LINE_WIDTH
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // 9.
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType =  VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1; 
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; 
        pipelineLayoutInfo.pushConstantRangeCount = 0; // optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // optional

        if(vkCreatePipelineLayout(core.defaultVulkanDevice->logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS){
            throw std::runtime_error("failed to create pipeline layout!");
        }

        // 10.
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};
        depthStencil.back = {};       

        // 11.
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        // shader stages info. just vertex and fragment:
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr; // optional
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // optional
        pipelineInfo.basePipelineIndex = -1; // optional
        pipelineInfo.pDepthStencilState = &depthStencil;

        if(vkCreateGraphicsPipelines(core.defaultVulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS){
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(core.defaultVulkanDevice->logicalDevice, vertShaderModule, nullptr);
        vkDestroyShaderModule(core.defaultVulkanDevice->logicalDevice, fragShaderModule, nullptr);
    }

    /*
    * Render Pass
    *  - Specify how many color and depth buffers there will be, how many samples to use for each of them,
    *    and how their contents should be handled throughout the rendering operations.
    * 
    * The dependency helps wait for the color to be available??
    */
    void createRenderPass(){
        
        VkAttachmentDescription colorAttachment {};
        colorAttachment.format = core.defaultSwapchain.colorFormat;
        colorAttachment.samples = core.defaultVulkanDevice->getMaxUsableSampleCount();
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAttachment {};
        depthAttachment.format = core.defaultVulkanDevice->getSupportedDepthFormat(false);
        depthAttachment.samples = core.defaultVulkanDevice->getMaxUsableSampleCount();
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription colorAttachmentResolve{};
        colorAttachmentResolve.format = core.defaultSwapchain.colorFormat;
        colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0; // index of the above attachment, in the array passed to RenderPassCreateInfo
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1; // index of the above attachment
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
        VkAttachmentReference colorAttachmentResolveRef{};
        colorAttachmentResolveRef.attachment = 2; // index of the above attachment, in the array passed to RenderPassCreateInfo
        colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pResolveAttachments = &colorAttachmentResolveRef;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };

        VkSubpassDependency dependency {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if(vkCreateRenderPass(core.defaultVulkanDevice->logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass");
        }
    }

    /*
    * FrameBuffer: A portion of RAM containing a bitmap that drives a video display
    * The attachments specified during render pass creation are bound by wrapping them in a VkFramebuffer object.
    * A framebuffer object references all the VkImageView objects that represent the attachments.
    * Currently, we only are using the color attachment. However, the image that we have to use for the
    * attachment depends on which image the swap chain returns when we retrieve one for presentation.
    * That means we have to create a framebuffer for all the images in the swap chain and use
    * the one that corresponds to the retrieved image at drawing time.
    * 
    * Essentially the place to throw the data about images.
    */
    void createFramebuffers() {
        swapChainFramebuffers.resize(core.defaultSwapchain.buffers.size());

        for(size_t i = 0;  i < core.defaultSwapchain.buffers.size(); i++) {

            std::array<VkImageView, 3> attachments = {
                sampleRenderTarget.view,
                depthStencil.view,
                core.defaultSwapchain.buffers[i].view
            };

            VkFramebufferCreateInfo framebufferInfo {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass  = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = core.defaultSwapchain.extent2D.width;
            framebufferInfo.height = core.defaultSwapchain.extent2D.height;
            framebufferInfo.layers = 1;

            if(vkCreateFramebuffer(core.defaultVulkanDevice->logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer.");
            }
        }
    }

    /*
    * Command pools manage the memory that is used to store the buffers and command buffers are allocated from them.
    */
    void createCommandPools() {
        graphicsCommandPool = core.defaultVulkanDevice->createCommandPool(
            core.defaultVulkanDevice->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT),
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        );

        transferCommandPool = core.defaultVulkanDevice->createCommandPool(
            core.defaultVulkanDevice->getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT),
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
        );
    }

    /*
    * Create color resources
    */
    void createColorResources() {

        VkFormat colorFormat = core.defaultSwapchain.colorFormat;

        VkImageCreateInfo imageCI {};
        imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.format = colorFormat;
        imageCI.extent.width = core.defaultSwapchain.extent2D.width;
        imageCI.extent.height = core.defaultSwapchain.extent2D.height;
        imageCI.extent.depth = 1;
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = 1;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageCI.samples = core.defaultVulkanDevice->getMaxUsableSampleCount();
        imageCI.flags = 0;

        VUB_CHECK_RESULT(vkCreateImage(core.defaultVulkanDevice->logicalDevice, &imageCI, nullptr, &sampleRenderTarget.image));

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(core.defaultVulkanDevice->logicalDevice, sampleRenderTarget.image, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = core.defaultVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VUB_CHECK_RESULT(vkAllocateMemory(core.defaultVulkanDevice->logicalDevice, &allocInfo, nullptr, &sampleRenderTarget.mem));
        VUB_CHECK_RESULT(vkBindImageMemory(core.defaultVulkanDevice->logicalDevice, sampleRenderTarget.image, sampleRenderTarget.mem, 0));

        VkImageViewCreateInfo imageViewCI{};
        imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCI.image = sampleRenderTarget.image;
        imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCI.format = colorFormat;
        imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCI.subresourceRange.baseMipLevel = 0;
        imageViewCI.subresourceRange.levelCount = 1;
        imageViewCI.subresourceRange.baseArrayLayer = 0;
        imageViewCI.subresourceRange.layerCount = 1;

        VUB_CHECK_RESULT(vkCreateImageView(core.defaultVulkanDevice->logicalDevice, &imageViewCI, nullptr, &sampleRenderTarget.view));
        
    }

    /*
    * Creates the image, view and memory for depth buffering
    */
    void createDepthResources() {
        VkFormat depthFormat = core.defaultVulkanDevice->getSupportedDepthFormat(false);

        VkImageCreateInfo imageCI {};
        imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.format = depthFormat;
        imageCI.extent.width = core.defaultSwapchain.extent2D.width;
        imageCI.extent.height = core.defaultSwapchain.extent2D.height;
        imageCI.extent.depth = 1;
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = 1;
        imageCI.samples = core.defaultVulkanDevice->getMaxUsableSampleCount();
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VUB_CHECK_RESULT(vkCreateImage(core.defaultVulkanDevice->logicalDevice, &imageCI, nullptr, &depthStencil.image));
        
        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(core.defaultVulkanDevice->logicalDevice, depthStencil.image, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = core.defaultVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VUB_CHECK_RESULT(vkAllocateMemory(core.defaultVulkanDevice->logicalDevice, &allocInfo, nullptr, &depthStencil.mem));
        VUB_CHECK_RESULT(vkBindImageMemory(core.defaultVulkanDevice->logicalDevice, depthStencil.image, depthStencil.mem, 0));

        VkImageViewCreateInfo imageViewCI{};
        imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCI.image = depthStencil.image;
        imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCI.format = depthFormat;
        imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        imageViewCI.subresourceRange.baseMipLevel = 0;
        imageViewCI.subresourceRange.levelCount = 1;
        imageViewCI.subresourceRange.baseArrayLayer = 0;
        imageViewCI.subresourceRange.layerCount = 1;

        VUB_CHECK_RESULT(vkCreateImageView(core.defaultVulkanDevice->logicalDevice, &imageViewCI, nullptr, &depthStencil.view));

        // TODO: is this transition needed ??
        transitionImageLayout(depthStencil.image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
    }

    // TODO move into device?
    /*
    * Checks if a specific format supports stencil buffer
    */
    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    /*
    * Creates the buffer for the index data
    */
    void createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(sphereModel.indices[0]) * sphereModel.indices.size();

        vub::Buffer stagingBuffer;
        VUB_CHECK_RESULT(core.defaultVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            &stagingBuffer,
            bufferSize,
            sphereModel.indices.data()
        ));

        VUB_CHECK_RESULT(core.defaultVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &indexBuffer,
            bufferSize
        ));

        core.defaultVulkanDevice->copyBuffer(&stagingBuffer, &indexBuffer, transferQueue, transferCommandPool);

        stagingBuffer.destroy();

    }

    /*
    * Buffers in Vulkan - regions of memory used for storing arbitrary data that can be read by the graphics card.
    *  - Can be used to store vertex data, and many other things
    *  - Need to allocate the space for buffers
    */
    void createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(sphereModel.vertices[0]) * sphereModel.vertices.size();

        vub::Buffer stagingBuffer;
        core.defaultVulkanDevice->createBuffer(
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &stagingBuffer,
                bufferSize,
                sphereModel.vertices.data());

        core.defaultVulkanDevice->createBuffer(
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &vertexBuffer,
                bufferSize
        );

        core.defaultVulkanDevice->copyBuffer(&stagingBuffer, &vertexBuffer, transferQueue, transferCommandPool);

        stagingBuffer.destroy();
    }

    /*
    * Creates the simple buffers used for uniform data
    */
    void createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

            core.defaultVulkanDevice->createBuffer(
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &uniformBuffers[i],
                    bufferSize);

        }
    }

    /*
    * Creates a pool in which to place descriptor sets
    */
    void createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if(vkCreateDescriptorPool(core.defaultVulkanDevice->logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    /*
     * Create vulkan descriptor pool.
     * Descriptors are ways for shaders to freely access resources like buffers and images.
     */
    void createImguiDescriptorPool() {
        std::vector<VkDescriptorPoolSize> poolSizes {
                {VK_DESCRIPTOR_TYPE_SAMPLER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
                {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
                {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
                {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)}
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if(vkCreateDescriptorPool(core.defaultVulkanDevice->logicalDevice, &poolInfo, nullptr, &imguiDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create imgui's descriptor pool!");
        }

    }

    /*
    * A descriptor set is a set of handles or pointers to resources. These resources are either buffers or images.
     * These descriptors are what the shaders read from.
    */
    void createDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        
        if(vkAllocateDescriptorSets(core.defaultVulkanDevice->logicalDevice, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i].buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture.view; // textureImageView;
            imageInfo.sampler = texture.sampler; // textureSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
            
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets[i];         
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;        
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets[i];         
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;        
            descriptorWrites[1].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(core.defaultVulkanDevice->logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    /*
    * Command Buffer - Used to record commands which can be subsequently submitted to a device queue for execution
    * Create the command buffer.
    */ 
    void createCommandBuffers(){
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = graphicsCommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if(vkAllocateCommandBuffers(core.defaultVulkanDevice->logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    /*
    * Writes the commands we want to execute into a command buffer
    */
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex ) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = core.defaultSwapchain.extent2D;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};
        
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        ImDrawData* drawData = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkBuffer vertexBuffers[] = {vertexBuffer.buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(sphereModel.indices.size()), 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    /*
    * Generates a transform to make geometry spin
    */
    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period> (currentTime - startTime).count();

        UniformBufferObject ubo{};
        auto model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(1.0, 1.0, 1.0));
        // model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.model = glm::rotate(model, time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = cam.matrices.view;
        ubo.proj = cam.matrices.perspective;


        uniformBuffers[currentImage].map(sizeof(ubo), 0);
        uniformBuffers[currentImage].copyTo(&ubo, sizeof(ubo));
        uniformBuffers[currentImage].unmap();
    }

    /*
    * Frame in Vulkan:
    *  1. Wait for previous frame to finish
    *  2. Acquire an image from the swap chain
    *  3. Record a command buffer which draws the scene onto the acquired image
    *  4. Submit the recorded command buffer
    *  5. Present the swap chain image
    * 
    *  Synchronization in Vulkan is explicit!!
    *  Semaphore - Use to add order between queue operations. Begins as unsignaled. We pass it to function A, and have function B wait for function A to signal the semaphore as finished or available
    *  Fence - Used to order execution on the CPU (or host). With a fence, we can have the CPU wait for a GPU action to finish.
    *  Queue operations - work we submit to a queue, either in a command buffer, or from within a function. Like the graphics or present code.
    *
    *  1. Uses fence to wait for prev. frame to finish. The wait function does the waiting. The reset tells the next frame to wait
    *  2. Gets an image from the defaultSwapchain, also checks if the defaultSwapchain needs refreshing
    *  2.5. Update the uniform buffer, before submitting
     * 2.6 Draw some imgui stuff
    *  3. Clear the command buffer so we can write to it
    *  4. Submit the command buffer, with a semaphore that waits to begin execution
    *  5. Present the frame
    * 
    *  We use multiple fences, semaphores, and command buffers to be able to render a frame, while recording the next
    */
    void drawFrame() {
        // 1.
        vkWaitForFences(core.defaultVulkanDevice->logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        // 2.
        uint32_t imageIndex;
        // VkResult result =  vkAcquireNextImageKHR(core.defaultVulkanDevice->logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        VkResult result = core.defaultSwapchain.acquireNextImage(imageAvailableSemaphores[currentFrame], &imageIndex);

        if(result == VK_ERROR_OUT_OF_DATE_KHR) {
            throw std::runtime_error("still need to handle recreating defaultSwapchain 1");
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR){
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // We only reset the fence if we are submitting work
        vkResetFences(core.defaultVulkanDevice->logicalDevice, 1, &inFlightFences[currentFrame]);

        // 2.5
        updateUniformBuffer(currentFrame);

        // 2.6
        /*ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();*/

        ImGui::ShowDemoWindow();
        ImGui::Render();

        // 3.
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        // 4.
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS){
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        // 5.
        result = core.defaultSwapchain.queuePresent(presentQueue, imageIndex, *signalSemaphores);

        if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            throw std::runtime_error("Need to still handle recreation of the defaultSwapchain.");
            //recreateSwapChain();
        } else if(result != VK_SUCCESS) {   
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    /*
    *   Creates 2 semaphores and a fence, explicitly 
    */
    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);        
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);        
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);        

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
            if(vkCreateSemaphore(core.defaultVulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(core.defaultVulkanDevice->logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(core.defaultVulkanDevice->logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS
            ) {
                throw std::runtime_error("failed to create semaphores!");
            }
        }
    }

    /*
    * Create and begin recording a command buffer in a pool
    */
    VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(core.defaultVulkanDevice->logicalDevice, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    /*
    * End, submit and free command buffer
    */
    void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool commandPool, uint32_t queueIndices) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        uint32_t graphicsQueueFamilyIndex = core.defaultVulkanDevice->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);

        if(queueIndices == graphicsQueueFamilyIndex){
            vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(graphicsQueue);
        }else{
            vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(transferQueue);
        }

        vkFreeCommandBuffers(core.defaultVulkanDevice->logicalDevice, commandPool, 1, &commandBuffer);
    }

    /*
    * Creates a pipeline barrier so the image's layout can be set. 
    * Images need to be laid out correctly for the render pass/copy
    */
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(graphicsCommandPool);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;

        if(newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            
            if(hasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, 
            destinationStage, 
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        uint32_t graphicsQueueFamilyIndex = core.defaultVulkanDevice->getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);

        endSingleTimeCommands(commandBuffer, graphicsCommandPool, graphicsQueueFamilyIndex);
    }

    /*
    * Reads in a file and return the byte data
    */
    static std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary); // ate -> start at end of file. binary ->  is a binary format file

        if(!file.is_open()){
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        
        file.close();

        return buffer;
    }   

};

int main() {
    std::cout << "HELLO WORLD" << std::endl;

    material mat;
    mat.test_load_spv();

    HelloTriangleApplication app;

    try{
        app.run();
    } catch(const std::exception& e){
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}