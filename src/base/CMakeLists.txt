
add_library(
	base
	"file_system.hpp"
	"file_system.cpp"
	"memory.hpp"
	"memory.cpp"
	"platform.hpp"
	"service.hpp"
	"service.cpp"
	"array.hpp"
    "assert.hpp"
	"log.hpp"
	"log.cpp"
	"data_structures.hpp"
	"data_structures.cpp"
	"string.hpp"
	"string.cpp"
	"hash_map.hpp"
	"bit.hpp"
	"bit.cpp"
	"process.hpp"
	"process.cpp"
	"resource_manager.hpp"
	"resource_manager.cpp"
	"numerics.hpp"
	"numerics.cpp"
	"color.hpp"
	"color.cpp"
	"gltf.hpp"
	"gltf.cpp"
	"time.hpp"
	"time.cpp"
	"camera.hpp"
	"camera.cpp"
)

# necessary libraries
target_link_libraries(base tlsf)
target_link_libraries(base cglm)

# necessary header
target_include_directories(base PUBLIC ../../deps/wyhash)
target_include_directories(base PUBLIC ../../deps/json/single_include/nlohmann)

# config
target_include_directories(base PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
