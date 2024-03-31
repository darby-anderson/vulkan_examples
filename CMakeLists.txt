cmake_minimum_required(VERSION 3.20)
project(VulkanExamples)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# shader data -> CONFIG
set(PUFFIN_SHADER_FOLDER ${PROJECT_SOURCE_DIR}/shaders/)
configure_file(puffin_config.h.in puffin_config.h)

add_executable(${PROJECT_NAME} src/main.cpp)

# INCLUDE CONFIG FILE
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

# dependencies

# GLFW - https://www.glfw.org/docs/latest/build_guide.html#build_link_cmake_source
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(deps/glfw)
target_link_libraries(${PROJECT_NAME} glfw)

# CGLM - https://github.com/recp/cglm
add_subdirectory(deps/cglm-0.9.1)
target_link_libraries(${PROJECT_NAME} cglm)

# GLM - https://github.com/g-truc/glm/blob/master/manual.md#-15-finding-glm-with-cmake
add_subdirectory(deps/glm)
target_link_libraries(${PROJECT_NAME} glm::glm)

# IMGUI - https://github.com/ocornut/imgui/blob/master/examples/example_glfw_vulkan/CMakeLists.txt
set(IMGUI_DIR deps/imgui)
include_directories(${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_sources(${PROJECT_NAME} PRIVATE
		${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
		${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
		${IMGUI_DIR}/imgui.cpp
		${IMGUI_DIR}/imgui_draw.cpp
		${IMGUI_DIR}/imgui_demo.cpp
		${IMGUI_DIR}/imgui_demo.cpp
		${IMGUI_DIR}/imgui_tables.cpp
		${IMGUI_DIR}/imgui_widgets.cpp
		)

# JSON - https://github.com/nlohmann/json#embedded
include_directories(deps/json/single_include/nlohmann)
#set(JSON_BuildTests OFF CACHE INTERNAL "")
#add_subdirectory(deps/json)
#target_link_libraries(${PROJECT_NAME} nlohmann_json::nlohmann_json)

# TINYOBJLOADER
add_subdirectory(deps/tinyobjloader)
target_link_libraries(${PROJECT_NAME} tinyobjloader)

# SPIRV-Cross
# add_subdirectory(deps/SPIRV-Cross)
# target_link_libraries(${PROJECT_NAME} spirv-cross-glsl spirv-cross-cpp spirv-cross-reflect)

# TLSF
add_subdirectory(deps/tlsf)
target_link_libraries(${PROJECT_NAME} tlsf)

# TRACY - https://github.com/wolfpld/tracy -> documentation -> first steps -> cmake integration
option(TRACY_ENABLE "" ON)
option(TRACY_ON_DEMAND "" ON)
add_subdirectory(deps/tracy-0.10)
target_link_libraries(${PROJECT_NAME} Tracy::TracyClient)

# WYHASH
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/deps/wyhash)

# STB
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/deps/stb)

# VMA
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/deps/VulkanMemoryAllocator/include)

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

# BASE
add_subdirectory(src/base)
# target_link_libraries(${PROJECT_NAME} src/base)
target_link_libraries(${PROJECT_NAME} base)
target_include_directories(${PROJECT_NAME} PUBLIC "src/base")

# GRAPHICS
add_subdirectory(src/graphics)
# target_link_libraries(${PROJECT_NAME} src/graphics)
target_link_libraries(${PROJECT_NAME} graphics)
target_include_directories(${PROJECT_NAME} PUBLIC "src/graphics")

# APPLICATION
add_subdirectory(src/application)
# target_link_libraries(${PROJECT_NAME} src/application)
target_link_libraries(${PROJECT_NAME} application)
target_include_directories(${PROJECT_NAME} PUBLIC "src/application")