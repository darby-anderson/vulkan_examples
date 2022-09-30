file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" "../external/imgui/*.cpp")

add_library(base STATIC ${BASE_SRC})

target_link_libraries( base 
	PRIVATE 
	Vulkan::Vulkan
)