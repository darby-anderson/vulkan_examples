add_library(
        application
        "../graveyard/camera.hpp"
        "../graveyard/camera.cpp"
        "window.hpp"
        "window.cpp"
        "input.hpp"
        "input.cpp"
        "keys.hpp"
        "application.hpp"
        "application.cpp"
        "game_camera.hpp"
        "game_camera.cpp"
)


target_link_libraries(
        application
        PRIVATE
        base
        graphics
        glm::glm
        glfw
)

# internal headers
target_include_directories(application PUBLIC ../base)
target_include_directories(application PUBLIC ../graphics)

# config
target_include_directories(application PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
