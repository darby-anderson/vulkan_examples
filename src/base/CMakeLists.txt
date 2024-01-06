
add_library(
	base
	"file_system.hpp"
	"file_system.cpp"
	"memory.hpp"
	"memory.cpp"
	"platform.hpp"
	"service.hpp"
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
	"numerics.hpp"
	"numerics.cpp"
	"color.hpp"
	"color.cpp"
	"gltf.hpp"
	"gltf.cpp"
	"time.hpp"
	"time.cpp"
)

target_link_libraries(base tlsf)

target_include_directories(base PUBLIC ../../deps/wyhash)
target_include_directories(base PUBLIC ../../deps/json/single_include/nlohmann)