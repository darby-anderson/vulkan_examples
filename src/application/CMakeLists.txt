add_library(
        application
        "camera.hpp"
        "camera.cpp"
        "window.hpp"
        "window.cpp"
)


target_link_libraries(
        application
        PRIVATE
        base
        glm::glm
        glfw
)

target_include_directories(graphics PUBLIC ../base)