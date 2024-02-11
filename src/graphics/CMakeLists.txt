
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
        "renderer.hpp"
        "renderer.cpp"
        "puffin_imgui.hpp"
        "puffin_imgui.cpp"
        "gpu_profiler.hpp"
        "gpu_profiler.cpp"
        "spirv_parser.cpp"
        "spriv_parser.hpp"
)


target_link_libraries(
        graphics
        PRIVATE
        base
        Vulkan::Vulkan
        glfw
        glm::glm
        tinyobjloader
        # spirv-cross-glsl spirv-cross-cpp spirv-cross-reflect
)

# header external libraries
target_include_directories(graphics PUBLIC ../../deps/stb)
target_include_directories(graphics PUBLIC ../../deps/VulkanMemoryAllocator/include)

# config - CMAKE_BINARY_DIR is the main directory, where the config lives
target_include_directories(graphics PUBLIC "${CMAKE_BINARY_DIR}")


# internal headers
target_include_directories(graphics PUBLIC ../base)