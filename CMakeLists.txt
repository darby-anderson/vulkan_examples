cmake_minimum_required(VERSION 3.20)
set(CMAKE_MODULE_PATH ${CMAKE})

set(
	CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
	CACHE STRING ""
)

set(VCPKG_TARGET_TRIPLET "x86-windows")

set(VCPKG_FEATURE_FLAGS "versions")
project(main VERSION 0.1)

include_directories(${PROJECT_SOURCE_DIR}/base)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/libs/VulkanMemoryAllocatord.lib)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/external_includes)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)

# window support
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# linear alg 
find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)

# Load OBJ files
find_package(tinyobjloader CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE tinyobjloader::tinyobjloader)

# Loads img files
find_path(STB_INCLUDE_DIRS "stb.h")
target_include_directories(${PROJECT_NAME} PRIVATE {STB_INCLUDE_DIRS})

add_subdirectory(base)