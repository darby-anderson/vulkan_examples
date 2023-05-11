add_library(
        application
        "camera.hpp"
        "camera.cpp"
        window.hpp)


target_link_libraries(
        application
        PRIVATE
        glm::glm
)