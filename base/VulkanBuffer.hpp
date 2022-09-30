/*
* Partially based on Sascha Willem's implementation: https://github.com/SaschaWillems/Vulkan
*/

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <assert.h>

namespace vub {

    /**
     * @brief Encapsulates access to a Vulkan buffer backed up by device memory 
     * @note To be filled by an external source like the VulkanDevice
     */
    struct Buffer
    {
        VkDevice device;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDescriptorBufferInfo descriptor;
        VkDeviceSize size = 0;
        VkDeviceSize alignment = 0;
        void* mapped = nullptr;
        // Usage flags to be filled by external source at buffer creation
        VkBufferUsageFlags usageFlags;
        // Memory property flags to be filled by external source at buffer creation
        VkMemoryPropertyFlags memoryPropertyFlags;
        VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void unmap();
        VkResult bind(VkDeviceSize offset = 0);
        void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void copyTo(void* data, VkDeviceSize size);
        VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void destroy();
    };
}