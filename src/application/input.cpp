//
// Created by darby on 3/5/2023.
//

#include <cstring>

#include "input.hpp"

namespace puffin {

    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        input* inputHandle = static_cast<input*>(glfwGetWindowUserPointer(window));

        if(action == GLFW_PRESS) {
            inputHandle->keys[key] = 1;
            inputHandle->first_frame_keys[key] = 1;
        } else if(action == GLFW_RELEASE) {
            inputHandle->keys[key] = 0;
            inputHandle->released_keys[key] = 1;
        }
    }

    void input::init(Window* window) {
        this->window = window;

        glfwSetWindowUserPointer(this->window->glfwWindow, reinterpret_cast<void*>(this));
        glfwSetKeyCallback(this->window->glfwWindow, keyCallback);

        memset(keys, 0, KEY_COUNT);
        memset(first_frame_keys, 0, KEY_COUNT);
        memset(released_keys, 0, KEY_COUNT);
    }

    void input::start_new_frame() {
        for(u32 i = 0; i < KEY_COUNT; i++) {
            released_keys[i] = 0;
            first_frame_keys[i] = 0;
        }
    }

    bool input::is_key_down(Key key) {
        return keys[key] && has_focus();
    }

    bool input::is_key_just_pressed(Key key) {
        return first_frame_keys[key] && has_focus();
    }

    bool input::is_key_just_released(Key key) {
        return released_keys[key] && has_focus();
    }

    bool input::has_focus() {
        return glfwGetWindowAttrib(window->glfwWindow, GLFW_FOCUSED);
    }
}