# set(VCPKG_TARGET_TRIPLET "x86-windows")
# set(VCPKG_FEATURE_FLAGS "versions")

# set(CMAKE_TOOLCHAIN_FILE "F:/DevTools/vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

message(STATUS "STARTING")

cmake_minimum_required(VERSION 3.20)
project(VulkanExamples)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# dependencies
find_package(tinyobjloader CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
# find_package(imgui REQUIRED)

find_path(STB_INCLUDE_DIRS "stb.h")

add_executable(${PROJECT_NAME} main.cpp)

# target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/libs/VulkanMemoryAllocatord.lib)
# target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/external_includes)

message(STATUS "PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")

add_subdirectory(base)

target_link_libraries( ${PROJECT_NAME} 
	PUBLIC	
	Vulkan::Vulkan
	glfw
	glm::glm
	tinyobjloader::tinyobjloader
	unofficial::vulkan-memory-allocator::vulkan-memory-allocator
	base
)

target_include_directories(
	${PROJECT_NAME} 
	PUBLIC	
	{STB_INCLUDE_DIRS}
	"${PROJECT_BINARY_DIR}"
	"${PROJECT_SOURCE_DIR}/base"
)
