/* Vulkan helper functions
*
* Partially based on Sascha Willems' work: https://github.com/SaschaWillems/Vulkan 
*/

#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <iostream>
#include <vector>
#include <set>
#include <cstdint>
#include <vector>
#include <cstring>


#include "VulkanInitializers.hpp"

// Custom define for better code readability
#define VK_FLAGS_NONE 0
#define DEFAULT_FENCE_TIMEOUT 100000000000

#define VUB_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		std::cout << "Fatal : VkResult is \"" << vub::tools::errorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << "\n"; \
		assert(res == VK_SUCCESS);																		\
	}																									\
}

namespace vub {
namespace tools {

/**
 * @brief Returns an error code as a string
 * 
 * @param errorCode The error code to be stringified
 * @return std::string 
 */
std::string errorString(VkResult errorCode);

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct VulkanPhysicalDeviceSettings {
	VkPhysicalDeviceType deviceType;
	bool supportsGeometryShader;
	bool supportsSamplerAnisotropy;
	std::vector<std::string> deviceExtensions;
	VkQueueFlags queueFlagsSupported;
};

VkPhysicalDevice findAppropriatePhysicalDevice(std::vector<VkPhysicalDevice> devices, VkSurfaceKHR surface, VulkanPhysicalDeviceSettings settings);

void setImageLayout(
	VkCommandBuffer cmdBuffer,
	VkImage image,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout,
	VkImageSubresourceRange subresourceRange,
	VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
);

void setImageLayout(
	VkCommandBuffer cmdBuffer,
	VkImage image,
	VkImageAspectFlags aspectMask,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout,
	VkImageSubresourceRange subresourceRange,
	VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
);

bool checkValidationLayerSupport(std::vector<const char*> validationLayers);
std::vector<const char*> getAllRequiredExtensions(bool addGLFWExtensions, bool enabledValidationLayers);
bool checkGLFWRequiredExtensionsSupport(std::vector<VkExtensionProperties> supportedExtensions);

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);


}
}


