//
// Created by darby on 3/5/2023.
//

#include <cstring>

#include "input.hpp"

namespace puffin {

    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        InputService* inputHandle = static_cast<InputService*>(glfwGetWindowUserPointer(window));

        if(action == GLFW_PRESS) {
            inputHandle->keys[key] = 1;
            inputHandle->first_frame_keys[key] = 1;
        } else if(action == GLFW_RELEASE) {
            inputHandle->keys[key] = 0;
            inputHandle->released_keys[key] = 1;
        }
    }

    void InputService::init(InputConfiguration* config) {
        window = config->window;

        window->set_window_user_pointer(reinterpret_cast<void*>(this));
        window->set_key_press_callback(key_callback);

        memset(keys, 0, KEY_COUNT);
        memset(first_frame_keys, 0, KEY_COUNT);
        memset(released_keys, 0, KEY_COUNT);
    }

    void InputService::start_new_frame() {
        for(u32 i = 0; i < KEY_COUNT; i++) {
            released_keys[i] = 0;
            first_frame_keys[i] = 0;
        }
    }

    bool InputService::is_key_down(Key key) {
        return keys[key] && window->has_focus();
    }

    bool InputService::is_key_just_pressed(Key key) {
        return first_frame_keys[key] && window->has_focus();
    }

    bool InputService::is_key_just_released(Key key) {
        return released_keys[key] && window->has_focus();
    }

}