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

        window = glfwCreateWindow(config.width, config.height, "Puffin::Vulkan", nullptr, nullptr);

        int window_width, window_height;
        glfwGetWindowSize(window, &window_width, &window_height);
        width = (u32) window_width;
        height = (u32) window_height;

        // Assigning this for outside use
        platform_handle = window;

        // OS Messages
        os_messages_callbacks.init(config.allocator, 4);
        os_messages_callbacks_data.init(config.allocator, 4);


        const GLFWvidmode* mode = glfwGetVideoMode()
        display_refresh = glfwGetMon

    }
}
