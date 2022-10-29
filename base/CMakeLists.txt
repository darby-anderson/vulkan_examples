# file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" "../external/imgui/*.cpp")

# add_library(base STATIC ${BASE_SRC})
add_library(
	base 
	"camera.hpp"
	"VulkanInitializers.hpp"
	"VulkanBuffer.hpp"
	"VulkanBuffer.cpp"
	"VulkanDevice.hpp"
	"VulkanDevice.cpp"
	"VulkanTexture.hpp"
	"VulkanTexture.cpp"
	"VulkanTools.hpp"
	"VulkanTools.cpp"
)

target_link_libraries( base 
	PRIVATE 
	Vulkan::Vulkan
)