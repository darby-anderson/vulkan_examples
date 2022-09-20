# set(VCPKG_TARGET_TRIPLET "x86-windows")
# set(VCPKG_FEATURE_FLAGS "versions")

set(CMAKE_TOOLCHAIN_FILE "F:/DevTools/vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(VulkanExamples)

# specify the C++ standard

# dependencies
find_package(tinyobjloader CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
# find_package(imgui REQUIRED)

find_path(STB_INCLUDE_DIRS "stb.h")

add_executable(${PROJECT_NAME} main.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/libs/VulkanMemoryAllocatord.lib)
# target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/external_includes)

target_include_directories(
	${PROJECT_NAME} 
	PRIVATE 
	{STB_INCLUDE_DIRS}
	${PROJECT_SOURCE_DIR}/base
)

target_link_libraries( ${PROJECT_NAME} 
	PRIVATE 
	Vulkan::Vulkan
	glfw
	glm::glm
	tinyobjloader::tinyobjloader
	unofficial::vulkan-memory-allocator::vulkan-memory-allocator
)

add_subdirectory(base)