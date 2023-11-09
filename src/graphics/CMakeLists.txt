# file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" "../external/imgui/*.cpp")

# add_library(base STATIC ${BASE_SRC})
add_library(
        graphics
        "model.hpp"
        "model.cpp"
        "vertex.hpp"
        "vulkan_resources.hpp"
        "vulkan_resources.cpp"
        "gpu_enum.hpp"
        "gpu_device.hpp"
        "gpu_device.cpp"
        "command_buffer.hpp"
        "command_buffer.cpp"
)


target_link_libraries(
        graphics
        PRIVATE
        base
        Vulkan::Vulkan
        glfw
        glm::glm
        tinyobjloader
        spirv-cross-glsl spirv-cross-cpp spirv-cross-reflect
)

# header external libraries
target_include_directories(graphics PUBLIC ../../deps/stb)
target_include_directories(graphics PUBLIC ../../deps/VulkanMemoryAllocator/include)

# internal headers
target_include_directories(graphics PUBLIC ../base)