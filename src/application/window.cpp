//
// Created by darby on 5/15/2023.
//

#include "window.hpp"

#include "assert.hpp"

static GLFWwindow* window = nullptr;
static GLFWmonitor* monitor = nullptr;

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

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if(mode != nullptr) {
            display_refresh = mode->refreshRate;
        }
    }

    bool Window::has_focus() {
        return glfwGetWindowAttrib(window, GLFW_FOCUSED);
    }

    void Window::shutdown() {
        os_messages_callbacks_data.shutdown();
        os_messages_callbacks.shutdown();

        glfwDestroyWindow(window);
        glfwTerminate();
   }

    void Window::set_fullscreen(bool value) {
        if(value) {
            // TODO SET GLFW FULLSCREEN
        } else {
            // TODO SET GLFW NOT FULLSCREEN
        }
    }

    void Window::handle_os_messages() {
        // TODO
    }

    void Window::register_os_messages_callback(OsMessagesCallback callback, void* user_data) {
        os_messages_callbacks.push(callback);
        os_messages_callbacks_data.push(user_data);
    }

    void Window::deregister_os_messages_callback(OsMessagesCallback callback) {
        PASSERTM(os_messages_callbacks.size < 8, "This array is too big for a linear search.");

        for(u32 i = 0; i < os_messages_callbacks.size; i++) {
            if(os_messages_callbacks[i] == callback) {
                os_messages_callbacks.delete_swap(i);
                os_messages_callbacks_data.delete_swap(i);
            }
        }

    }

    void Window::center_mouse(bool dragging) {
        // TODO - CENTER MOUSE WITH GLFW
    }

    void Window::set_window_user_pointer(void* user) {
        glfwSetWindowUserPointer(window, user);
    }

    void Window::set_key_press_callback(GLFWkeyfun callback) {
        glfwSetKeyCallback(window, callback);
    }

}
