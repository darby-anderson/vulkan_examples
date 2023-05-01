add_library(
        application
        "camera.hpp"
        "camera.cpp"
)


target_link_libraries(
        application
        PRIVATE
        glm::glm
)