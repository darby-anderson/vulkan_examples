add_library(
        application
        "camera.hpp"
        "camera.cpp"
        "window.hpp"
        "window.cpp"
        "input.hpp"
        "input.cpp"
        "keys.hpp"
        "application.hpp"
        "application.cpp"
)


target_link_libraries(
        application
        PRIVATE
        base
        graphics
        glm::glm
        glfw
)

target_include_directories(application PUBLIC ../base)
target_include_directories(application PUBLIC ../graphics)