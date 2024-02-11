//
// Created by darby on 3/5/2023.
//

#include <cstring>
#include <cmath>

#include "input.hpp"

namespace puffin {


    static void key_pressed_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        InputService* input = static_cast<InputService*>(glfwGetWindowUserPointer(window));

        if(action == GLFW_PRESS) {
            input->keys[key] = 1;
            input->first_frame_keys[key] = 1;
        } else if(action == GLFW_RELEASE) {
            input->keys[key] = 0;
            input->released_keys[key] = 1;
        }
    }

    static void mouse_button_pressed_callback(GLFWwindow* window, int button, int action, int mods) {
        InputService* input = static_cast<InputService*>(glfwGetWindowUserPointer(window));

        if(action == GLFW_PRESS) {
            input->mouse_buttons[button] = 1;
            input->first_frame_mouse_buttons[button] = 1;
        } else if(action == GLFW_RELEASE) {
            input->mouse_buttons[button] = 0;
            input->released_mouse_buttons[button] = 1;
        }
    }

    static void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
        InputService* input = static_cast<InputService*>(glfwGetWindowUserPointer(window));

        input->window->width = width;
        input->window->height = height;
        input->window->resized = true;
    }



    void InputService::init(InputConfiguration* config) {
        window = config->window;

        window->set_window_user_pointer(reinterpret_cast<void*>(this));
        window->set_key_press_callback(key_pressed_callback);
        window->set_mouse_button_press_callback(mouse_button_pressed_callback);
        window->set_framebuffer_resize_callback(framebuffer_resize_callback);

        memset(keys, 0, KEY_COUNT);
        memset(first_frame_keys, 0, KEY_COUNT);
        memset(released_keys, 0, KEY_COUNT);

        memset(mouse_buttons, 0, MOUSE_BUTTON_COUNT);
        memset(first_frame_mouse_buttons, 0, MOUSE_BUTTON_COUNT);
        memset(released_mouse_buttons, 0, MOUSE_BUTTON_COUNT);
    }

    void InputService::start_new_frame() {
        for(u32 i = 0; i < KEY_COUNT; i++) {
            released_keys[i] = 0;
            first_frame_keys[i] = 0;
        }

        for(u32 i = 0; i < MOUSE_BUTTON_COUNT; i++) {
            released_mouse_buttons[i] = 0;
            first_frame_mouse_buttons[i] = 0;
        }

        previous_mouse_position = mouse_position;
        mouse_position = window->get_window_cursor_position();

        for(u32 i = 0; i < MOUSE_BUTTON_COUNT; i++) {
            if(is_mouse_button_just_pressed( (MouseButton) i )) {
                mouse_clicked_position[i] = mouse_position;
            } else if(is_mouse_button_down( (MouseButton) i )) {
                f32 delta_x = mouse_position.x - mouse_clicked_position[i].x;
                f32 delta_y = mouse_position.y - mouse_clicked_position[i].y;
                mouse_drag_distance[i] = sqrtf((delta_x * delta_x) + (delta_y * delta_y));
            }
        }

    }

    bool InputService::is_mouse_button_down(MouseButton button) {
        return mouse_buttons[button] && window->has_focus();
    }

    bool InputService::is_mouse_button_just_pressed(MouseButton button) {
        return first_frame_mouse_buttons[button]&& window->has_focus();
    }

    bool InputService::is_mouse_button_just_released(MouseButton button) {
        return released_mouse_buttons[button] && window->has_focus();
    }

    static constexpr f32 k_mouse_drag_min_distance = 4.f;

    bool InputService::is_mouse_dragging(MouseButton button) {
        if(!mouse_buttons[button]) {
            return false;
        }

        return mouse_drag_distance[button] > k_mouse_drag_min_distance;
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