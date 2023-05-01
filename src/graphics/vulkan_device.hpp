/*
* Encapsulates a physical and logical device
*/
#pragma once

#include "VulkanBuffer.hpp"
#include "VulkanTools.hpp"
#include "vulkan_resources.hpp"

#include <vulkan/vulkan.h>
#include <algorithm>
#include <vector>
#include <string>
#include <iostream>

namespace puffin {

struct DeviceCreation {
    GLFWwindow*         window  = nullptr;
    uint32_t            width   = 1;
    uint32_t            height  = 1;

    DeviceCreation&     set_window(uint32_t width, uint32_t height, void* handle);
};

struct VulkanDevice {

    static VulkanDevice*        instance();

    // Init/Terminate methods
    void                        init(const DeviceCreation& creation);
    void                        shutdown();

    // Creation/Destruction of resources
    BufferHandle                create_buffer( const BufferCreation& creation);
    TextureHandle               create_texture( const TextureCreation& creation);
    // PipelineHandle              create_pipeline( const PipelineCreation& creation, const char* cache_path = nullptr );
    SamplerHandle               create_sampler( const SamplerCreation& creation);
    DescriptorSetLayoutHandle   creation_descriptor_set_layout(const DescriptorSetLayoutCreation& creation);
    DescriptorSetHandle         create_descriptor_set( const DescriptorSetCreation& creation);
    RenderPassHandle            create_render_pass( const RenderPassCreation& creation);
    ShaderStateHandle           create_shader_state( const ShaderStateCreation& creation);


    /** @brief Physical Device representation */
    VkPhysicalDevice physicalDevice;
    /** @brief Logical representation (application's view of the device) */
    VkDevice logicalDevice;
    /** @brief Properties of the physical device. Useful to check for limits */
    VkPhysicalDeviceProperties properties;
    /** @brief Features of the physical device that an app can check to see if supported */
    VkPhysicalDeviceFeatures features;
    /** @brief The features enabled for use on the physical device */
    VkPhysicalDeviceFeatures enabledFeatures;
    /** @brief Memory types and heaps on the physical device */
    VkPhysicalDeviceMemoryProperties memoryProperties;
    /** @brief Queue family properties of the physical device */
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    /** @brief List of extensions supported by the device */
    std::vector<std::string> supportedExtensions;
    /** @brief Default command pool for the graphics queue family index */
    VkCommandPool graphicsQueueCommandPool = VK_NULL_HANDLE;
    /** @brief Set to true when the debug marker extension is detected */
    bool enableDebugMarkers = false;
    /** @brief Result of device checks for bindless support */
    bool bindlessSupported = false;

    struct
    {
        uint32_t graphics;
        uint32_t compute;
        uint32_t transfer;
    } queueFamilyIndices;

    operator VkDevice() const {
        return logicalDevice;
    };

    explicit VulkanDevice(VkPhysicalDevice physicalDevice);
    ~VulkanDevice();

    uint32_t        getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound = nullptr) const;
    uint32_t        getPresentQueueFamilyIndex(VkSurfaceKHR surface) const;
    uint32_t        getQueueFamilyIndex(VkQueueFlags queueFlags) const;

    VkResult        createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures,
                                        std::vector<const char*> enabledExtensions,
                                        std::vector<const char*> enabledLayers, void *pNextChain,
                                        bool useSwapChain = true,
                                        VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

    VkResult        createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
                                 VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data = nullptr);

    VkResult        createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
                                 puffin::Buffer *buffer, VkDeviceSize size, void *data = nullptr);

    void copyBuffer(puffin::Buffer *src, puffin::Buffer *dst, VkQueue queue, VkCommandPool commandPool,
                    VkBufferCopy *copyRegion = nullptr);

    VkCommandPool   createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin = false);
    VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = false);
    void            flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true);
    void            flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);
    bool            extensionSupported(std::string extension);
    VkFormat        getSupportedDepthFormat(bool checkSamplingSupport);
    VkSampleCountFlagBits getMaxUsableSampleCount();
    uint32_t        getVulkanApiVersion();

};
}

