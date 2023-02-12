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
	"VulkanSwapchain.hpp"
	"VulkanSwapChain.cpp"
	"VulkanFrameBuffer.hpp"
	"defaultCore.hpp"
	"defaultCore.cpp"
	"model.hpp"
	"model.cpp"
	"vertex.hpp"
	"material.hpp"
	"material.cpp"
	"file_system.hpp")


target_link_libraries(
	base
	PRIVATE
	Vulkan::Vulkan
	glfw
	glm::glm
	tinyobjloader
	spirv-cross-glsl spirv-cross-cpp spirv-cross-reflect
)

target_include_directories(base PUBLIC ${PROJECT_SOURCE_DIR}/deps/stb)