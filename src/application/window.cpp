//
// Created by darby on 5/15/2023.
//

#include "window.hpp"

static GLFWwindow* window = nullptr;

namespace puffin {

    cstring* get_glfw_error() {
        cstring* err = nullptr;
        glfwGetError(err);
        return err;
    }

    void Window::init(void* configuration_) {
        p_print("Window Service \n");

        if(glfwInit() == GLFW_FALSE) {
            p_print("GLFW Error \n", *get_glfw_error());
            return;
        }

        WindowConfiguration& config = *(WindowConfiguration*) configuration_;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);


        Service::init(configuration);
    }
}
