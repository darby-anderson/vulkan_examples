/*
* Vulkan device class
* Encapsulates a physical Vulkan device and its logical representation
*
* Code partially based on Sascha Willems' project: https://github.com/SaschaWillems/Vulkan
*/

#include "vulkan_device.hpp"
#include "VulkanInitializers.hpp"
#include <unordered_set>
#include <stdexcept>

namespace puffin
{

/**
 * @brief Default Construct a new Vulkan Device:: Vulkan Device object
 * 
 * @param physicalDevice Physical device to be used 
 */
VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice)
{
    assert(physicalDevice);
    this->physicalDevice = physicalDevice;

    // Store properties, features and limits of the physcial device for later use
    // Device properties also contain limits and sparse properties
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    // Features should be checked by the main app before using
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    // Memory properties are used commonly when creating buffers
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    // Queue family properties, used for setting up requested queues upon device creation
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    queueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    // Get list of supported extensions
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
    if(extCount > 0) {
        std::vector<VkExtensionProperties> extensions(extCount);
        if(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS) {
            for(auto ext : extensions) {
                supportedExtensions.push_back(ext.extensionName);
            }
        }
    }

}

/**
 * @brief Default Destroy the Vulkan Device:: Vulkan Device object
 * 
 * @note frees the logical device and command pool
 */
VulkanDevice::~VulkanDevice(){
    if(graphicsQueueCommandPool) {
        vkDestroyCommandPool(logicalDevice, graphicsQueueCommandPool, nullptr);
    }
    if(logicalDevice) {
        vkDestroyDevice(logicalDevice, nullptr);
    }
}

/**
 * @brief Get the index of a memory type that has all the requested property bits set
 * 
 * @param typeBits Bit mask with bits set for each memory type supported by the resource to request for (from VkMemoryRequirements)
 * @param properties  Bit mask of properties for the memory type to request
 * @param memTypeFound  (Optional) Pointer to a bool that is set to true if a matching memory type has been found
 * 
 * @return uint32_t  Index of the requested memory type
 * 
 * @throw Throws an exception if memTypeFound is null and no memory type could be found that supports the requested properties
 */
uint32_t VulkanDevice::getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound) const {
    for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if((typeBits & 1) == 1) {
            if((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties){
                if(memTypeFound) {
                    *memTypeFound = VK_TRUE;
                }
                return i;
            }
        }
        typeBits >>= 1;
    }

    if(memTypeFound) {
        *memTypeFound = false;
        return 0;
    } else {
        throw std::runtime_error("Could not find a matching memory type");
    }
}

/**
 * @brief Get the index of a queue family that supports the requested queue flags
 * SRS - support the VkQueueFlags parameter for requesting multiple flags vs. VkQueueFlagBits for a single flag only
 * 
 * @param queueFlags  Queue flags to find a queue family index for
 * @return uint32_t Index of the queue family index that matches the flags
 * 
 * @throw Throws an exception if no queue family index could be found that supports the requested flags
 */
uint32_t VulkanDevice::getQueueFamilyIndex(VkQueueFlags queueFlags) const {
    // Dedicated queue for compute
    // Try to find a queue family index that supports compute but not graphics
    if((queueFlags & VK_QUEUE_COMPUTE_BIT) == queueFlags) {
        for(uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
            if((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)){
                    return i;
                }
        }
    }

    // Dedicated queue for transfer 
    // Try to find a queue family index that supports transfer but not graphics or compute
    if((queueFlags & VK_QUEUE_TRANSFER_BIT) == queueFlags) {
        for(uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
            if((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && 
                ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)){
                    return i;
                }
        }
    }

    // For other queue types than transfer, and compute, and all others, return the first queue that supports the flags
    for(uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
        if((queueFamilyProperties[i].queueFlags & queueFlags) == queueFlags) {
            return i;
        }
    }

    throw std::runtime_error("Could not find a matching queue family index");
}

uint32_t VulkanDevice::getPresentQueueFamilyIndex(VkSurfaceKHR surface) const {
    // For other queue types than transfer, and compute, and all others, return the first queue that supports the flags
    for(uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
    
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

        if(presentSupport) {
            return i;
        }
    }

    throw std::runtime_error("Could not find a present queue"); 
}


    /**
     * @brief Create the logical device based on the assigned physical device, also gets default queue family indices
     * 
     * @param enabledFeatures Can be used to enable certain features on device creation
     * @param enabledExtensions A list of extensions attempted to be enabled on the device level
     * @param enabledLayers A list of the validation layers to be activated
     * @param pNextChain (Optional) chain of pointers to extension structures
     * @param useSwapChain (Optional) Set to flase for ehealess rendering to omit the defaultSwapchain device extensions
     * @param requestedQueueTypes (Optional) Bit flags specifiying the queue types to be requested from the device 
     * @return VkResult 
     */
    VkResult VulkanDevice::createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, std::vector<const char*> enabledLayers, void *pNextChain, bool useSwapChain, VkQueueFlags requestedQueueTypes){

        // Desired queues need to be requested upon logical device creation
        // Due to differing queue family configurations of Vulkan implementations this can be tricky, 
        // particularly if the app requests different queue types

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos {};

        // Get queue family indices for the requested queue family types
        // Note that the indices may overlap depending on the implementation

        const float defaultQueuePriority(0.0f);

        // Graphics queue
        if(requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT) {
            queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
            VkDeviceQueueCreateInfo queueInfo{};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = queueFamilyIndices.graphics;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &defaultQueuePriority;
            queueCreateInfos.push_back(queueInfo);
        }else{
            queueFamilyIndices.graphics = 0;    
        }

        // Dedicated compute queue
        if(requestedQueueTypes & VK_QUEUE_COMPUTE_BIT) {
            queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
            if(queueFamilyIndices.compute != queueFamilyIndices.graphics) {
                // If compute family index differes, we need an additional queue create info for the compute queue
                VkDeviceQueueCreateInfo queueInfo{};
                queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueInfo.queueFamilyIndex = queueFamilyIndices.compute;
                queueInfo.queueCount = 1;
                queueInfo.pQueuePriorities = &defaultQueuePriority;
                queueCreateInfos.push_back(queueInfo);
            }
        } else {
            // Else use the same queue
            queueFamilyIndices.compute = queueFamilyIndices.graphics;
        }

        // Dedicated transfer queue
        if(requestedQueueTypes & VK_QUEUE_TRANSFER_BIT) {
            queueFamilyIndices.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
            if((queueFamilyIndices.transfer != queueFamilyIndices.graphics) && (queueFamilyIndices.transfer != queueFamilyIndices.compute)) {
                // If transfer family index differs, we need an additional queue create info for the transfer queue
                VkDeviceQueueCreateInfo queueInfo{};
                queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueInfo.queueFamilyIndex = queueFamilyIndices.transfer;
                queueInfo.queueCount = 1;
                queueInfo.pQueuePriorities = &defaultQueuePriority;
                queueCreateInfos.push_back(queueInfo);
            }
        } else {
            // Else we use the same queue
            queueFamilyIndices.transfer = queueFamilyIndices.graphics;
        }

        // Create the logical device representation
        std::vector<const char*> deviceExtensions(enabledExtensions);
        if(useSwapChain)
        {
            // If the device will be used for presenting to a display via a defaultSwapchain we need to request the defaultSwapchain extension
            deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }

        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

        // MASTERING VULKAN PROGRAMMING CH. 2 - Bindless
        VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
        indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

        VkPhysicalDeviceFeatures2 testBindlessPhysicalDeviceFeatures2{};
        testBindlessPhysicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        testBindlessPhysicalDeviceFeatures2.pNext = &indexingFeatures;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &testBindlessPhysicalDeviceFeatures2);

        bindlessSupported = indexingFeatures.descriptorBindingPartiallyBound &&
                            indexingFeatures.runtimeDescriptorArray;

        std::cout << "bindless support: " << bindlessSupported << std::endl;

        // If a pNext(Chain) has been passed, we need to add it to the device creation info
        VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
        physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        physicalDeviceFeatures2.features = enabledFeatures;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &physicalDeviceFeatures2);

        deviceCreateInfo.pEnabledFeatures = nullptr;
        deviceCreateInfo.pNext = &physicalDeviceFeatures2;

        if(bindlessSupported) {
            physicalDeviceFeatures2.pNext = &indexingFeatures;
            indexingFeatures.pNext = pNextChain;
        } else if(pNextChain) {
            physicalDeviceFeatures2.pNext = pNextChain;
        }

        // Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
        if(extensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
            deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
            enableDebugMarkers = true;
        }

        if(deviceExtensions.size() > 0) {
            for(const char * enabledExtension : deviceExtensions) {
                if(!extensionSupported(enabledExtension)) {
                    std::cerr << "Enabled device extension \"" << enabledExtension << "\" is not present at device level" << std::endl;
                }
            }

            deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
            deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
        }

        this->enabledFeatures = enabledFeatures;

        if(enabledLayers.size() > 0) {
            deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size());
            deviceCreateInfo.ppEnabledLayerNames = enabledLayers.data();        
        }

        VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice);
        if(result != VK_SUCCESS) {
            return result;
        }

        // Create a default command pool for graphics command buffers
        graphicsQueueCommandPool = createCommandPool(queueFamilyIndices.graphics);

        return result;
    }


    /**
     * @brief Create a buffer on the device 
     * 
     * @param usageFlags Usage flag bit mask for the buffer (i.e. index, vertex, uniform buffer)
     * @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
     * @param size Size of the buffer in bytes
     * @param buffer Pointer to the buffer handle acquired by the function
     * @param memory Pointer to the memory handle acquired by the function
     * @param data (Optional) Pointer to the data that should be copied to the buffer after creation (no data copied if not set)
     * @return VkResult Returns success if buffer handle and memory have been created and (optionally passed) data has been copied
     */
    VkResult VulkanDevice::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data) {

        // Create the buffer handle
        VkBufferCreateInfo bufferCreateInfo = puffin::initializers::bufferCreateInfo(usageFlags, size);
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VUB_CHECK_RESULT(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, buffer));

        // Create the memory backing up the buffer media
        VkMemoryRequirements memReqs;
        VkMemoryAllocateInfo memAlloc = puffin::initializers::memoryAllocateInfo();
        vkGetBufferMemoryRequirements(logicalDevice, *buffer, &memReqs);
        memAlloc.allocationSize = memReqs.size;

        // Find a memory type index that fits the properties of the buffer
        memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
        // If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
        VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
        if(usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
            memAlloc.pNext = &allocFlagsInfo;
        }

        VUB_CHECK_RESULT(vkAllocateMemory(logicalDevice, &memAlloc, nullptr, memory));

        if(data != nullptr) { 
            void* mapped;
            VUB_CHECK_RESULT(vkMapMemory(logicalDevice, *memory, 0, size, 0, &mapped));
            memcpy(mapped, data, size);
            // If host coherency hasn't been requested, do a manual flush to make writes visible
            // Needed since coherent memory is the same on all cache levels, while non-coherent may not be
            if((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
                VkMappedMemoryRange mappedRange = puffin::initializers::mappedMemoryRange();
                mappedRange.memory = *memory;
                mappedRange.offset = 0;
                mappedRange.size = size;
                vkFlushMappedMemoryRanges(logicalDevice, 1, &mappedRange);
            } 
            vkUnmapMemory(logicalDevice, *memory);
        }
        vkUnmapMemory(logicalDevice, *memory);
    
        VUB_CHECK_RESULT(vkBindBufferMemory(logicalDevice, *buffer, *memory, 0));

        return VK_SUCCESS;
    }

     /**
     * @brief Create a buffer on the device 
     * 
     * @param usageFlags Usage flag bit mask for the buffer (i.e. index, vertex, uniform buffer)
     * @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
     * @param buffer Pointer to a vub::Buffer object 
     * @param size Size of the buffer in bytes 
     * @param data (Optional) Pointer to the data that should be copied to the buffer after creation (no data copied if not set)
     * @return VkResult Returns success if buffer handle and memory have been created and (optionally passed) data has been copied
     */
    VkResult VulkanDevice::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, puffin::Buffer *buffer, VkDeviceSize size, void *data) {
        
        buffer->device = logicalDevice;
    
        // Create the buffer handle
        VkBufferCreateInfo bufferCreateInfo = puffin::initializers::bufferCreateInfo(usageFlags, size);
        VUB_CHECK_RESULT(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, &buffer->buffer));

        // Create the memory backing up the buffer handle
        VkMemoryRequirements memReqs;
        VkMemoryAllocateInfo memAlloc = puffin::initializers::memoryAllocateInfo();
        vkGetBufferMemoryRequirements(logicalDevice, buffer->buffer, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        // Find a memory type index that fits the properties of the buffer
        memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
        // If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enabled the appropriate flag during allocation
        VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
        if(usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
            memAlloc.pNext = &allocFlagsInfo;
        }
        VUB_CHECK_RESULT(vkAllocateMemory(logicalDevice, &memAlloc, nullptr, &buffer->memory));

        buffer->alignment = memReqs.alignment;
        buffer->size = size;
        buffer->usageFlags = usageFlags;
        buffer->memoryPropertyFlags = memoryPropertyFlags;

        // IF a pointer to the buffer data has been passed, map the buffer and copy over the data
        if(data != nullptr) {
            VUB_CHECK_RESULT(buffer->map());
            memcpy(buffer->mapped, data, size);
            if((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
                buffer->flush();
            }
            buffer->unmap();
        }

        // Initialize a default descriptor that covers the whole buffer size
        buffer->setupDescriptor();

        // Attach the memory to the buffer object
        return buffer->bind();
    }

    /**
     * @brief Copy Buffer data from src to dst using VkCmdCopyBuffer 
     * 
     * @param src Pointer to the source buffer to copy from
     * @param dst Pointer to the destination buffer to copy to
     * @param commandPool Pointer
     * @param copyRegion (Optional) pointer to a copy region, if NULL the whole buffer is copied
     * 
     * @note Source and destination pointers must have the appropriate transfer usage flags set (TRANSFER_SRC / TRANSFER_DST)
     */
    void VulkanDevice::copyBuffer(puffin::Buffer *src, puffin::Buffer *dst, VkQueue queue, VkCommandPool commandPool,
                                  VkBufferCopy *copyRegion) {
        assert(dst->size <= src->size);
        assert(src->buffer);

        VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, commandPool, true);
        VkBufferCopy bufferCopy{};
        if(copyRegion == nullptr) {
            bufferCopy.size = src->size;
        } else {
            bufferCopy = *copyRegion;
        }

        vkCmdCopyBuffer(copyCmd, src->buffer, dst->buffer, 1, &bufferCopy);

        flushCommandBuffer(copyCmd, queue, commandPool);
    }

    /**
     * @brief Creates a command pool for allocating command buffers from
     * 
     * @param queueFamilyIndex Family index of the queue to create the command pool for
     * @param createFlags (Optional) Command pool creation flags (Defaults to VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
     *
     * @note Command buffers allocated from the creation pool can only be submitted to a queue with the same family index
     *  @return VkCommandPool 
     */
    VkCommandPool VulkanDevice::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags) {
        VkCommandPoolCreateInfo cmdPoolInfo{};
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
        cmdPoolInfo.flags = createFlags;
        VkCommandPool cmdPool;
        VUB_CHECK_RESULT(vkCreateCommandPool(logicalDevice, &cmdPoolInfo, nullptr, &cmdPool));
        return cmdPool;
    }
    
    /**
     * @brief Allocate a command buffer from the specified command pool
     * 
     * @param level level of the new command buffer (PRIMARY or SECONDARY)
     * @param pool Command pool from which the command buffer will be allocated
     * @param begin (Optional) if true, will start recording on the new command buffer (vkBeginCommandBuffer) (Defaults to false) 
     * @return VkCommandBuffer 
     */
    VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin) {
        VkCommandBufferAllocateInfo cmdBufAllocateInfo = puffin::initializers::commandBufferAllocateInfo(pool, level, 1);
        VkCommandBuffer cmdBuffer;
        VUB_CHECK_RESULT(vkAllocateCommandBuffers(logicalDevice, &cmdBufAllocateInfo, &cmdBuffer));
        // If requested, start recording for the new command buffer
        if(begin) {
            VkCommandBufferBeginInfo cmdBufInfo = puffin::initializers::commandBufferBeginInfo();
            VUB_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
        }
        return cmdBuffer;
    }

    VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, bool begin) {
        return createCommandBuffer(level, graphicsQueueCommandPool, begin);
    }

    /**
     * @brief Finish command buffer recording and submit it to a queue 
     * 
     * @param commandBuffer Command buffer to flush
     * @param queue Queue to submit the cmd buffer to
     * @param pool Command pool in which the buffer was created
     * @param free (Optional) Free the command buffer once it has been submitted (Defaults to true)
     * 
     * @note The queue that the command buffer is submiteed to must be from the same family index as the pool it was allocated from
     * @note Uses a fence to ensure command buffer has finished executing
     * 
     */
    void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free) {
        if(commandBuffer == VK_NULL_HANDLE) {
            return;
        }

        VUB_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo = puffin::initializers::submitInfo();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        // Create fence to ensure that the command buffer has finished executing
        VkFenceCreateInfo fenceInfo = puffin::initializers::fenceCreateInfo(VK_FLAGS_NONE);
        VkFence fence;
        VUB_CHECK_RESULT(vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence));
        // Submit to the queue
        VUB_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
        // Wait for the fence to signal that command buffer has finished executing
        VUB_CHECK_RESULT(vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
        vkDestroyFence(logicalDevice, fence, nullptr);
        if(free) {
            vkFreeCommandBuffers(logicalDevice, pool, 1, &commandBuffer);
        }
    }

    void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free) {
        return flushCommandBuffer(commandBuffer, queue, graphicsQueueCommandPool, free);
    }

    /**
     * @brief Checks if the specified extension is supported by the device
     * 
     * @param extension the name of the extension to check
     * @return true if the extension is supported (present in the list read at device creation time)
     * @return false  the extension is not supported
     * 
     * @throw Throws if no matching depth format fits the requirements
     */
    bool VulkanDevice::extensionSupported(std::string extension) {
        return (std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) != supportedExtensions.end());
    }

    /**
     * @brief Select the best depth format for this device from a list of possible depth (or stencil) formats
     * 
     * @param checkSamplingSupport Checks if the format can be sampled from (like shader reads)
     * @return VkFormat 
     * 
     * @throw Throws an exception when no depth format fits our requirements
     */
    VkFormat VulkanDevice::getSupportedDepthFormat(bool checkSamplingSupport) {
        // All depth formats may be optional, so we need to find a suitable depth format to use
        std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };
    
        for(auto& format : depthFormats) {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
            // Format must support depth stencil for optimal tiling
            if(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                if(checkSamplingSupport) {
                    if(!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                        continue;
                    }
                }

                return format;
            }
        }
        throw std::runtime_error("Could not find a matching depth format"); 
    }


    /**
     * @brief Get the Max Usable Sample Count available to this device 
     * 
     * @return VkSampleCountFlagBits 
     */
    VkSampleCountFlagBits VulkanDevice::getMaxUsableSampleCount() {
        VkSampleCountFlags counts = this->properties.limits.framebufferColorSampleCounts & 
                                    this->properties.limits.framebufferDepthSampleCounts; 
    
        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
        
        return VK_SAMPLE_COUNT_1_BIT;
    }

    /**
     * @brief Get the Vulkan Api Version 
     * 
     * @return uint32_t 
     */
    uint32_t VulkanDevice::getVulkanApiVersion() {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(this->physicalDevice, &physicalDeviceProperties);

        return physicalDeviceProperties.apiVersion;
    }

};