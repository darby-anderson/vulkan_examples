
add_library(
	base
	"file_system.hpp"
	"file_system.cpp"
	"memory.hpp"
	"memory.cpp"
	"platform.hpp"
	"service.hpp"
	"array.hpp"
	"array.cpp"
	"assert.hpp"
	"log.hpp"
	"log.cpp"
)

target_link_libraries(base tlsf)