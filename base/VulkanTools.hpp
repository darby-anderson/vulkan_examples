/* Vulkan helper functions
*
* Partially based on Sascha Willems' work: https://github.com/SaschaWillems/Vulkan 
*/

#pragma once

#include <vulkan/vulkan.h>
#include <iostream>

// Custom define for better code readability
#define VK_FLAGS_NONE 0
#define DEFAULT_FENCE_TIMEOUT 100000000000

/*#define VK_CHECK_RESULT(f)
{
    VkResult res = (f);
    if(res != VK_SUCCESS) {
        std::cout << "Fatal: VkResult is \"" %s "\" in %s at line %d", vub::tools:errorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl;
        assert(res == VK_SUCCESS); 
    }
}*/

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

}
}


