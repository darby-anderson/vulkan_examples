add_library(
        application
        "camera.hpp"
        "camera.cpp"
        "window.hpp"
        "window.cpp"
        "input.hpp"
        "input.cpp"
        "keys.hpp"
)


target_link_libraries(
        application
        PRIVATE
        base
        glm::glm
        glfw
)

target_include_directories(application PUBLIC ../base)