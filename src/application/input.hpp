//
// Created by darby on 3/5/2023.
//

#pragma once

#include "window.hpp"
#include "service.hpp"
#include "platform.hpp"
#include "keys.hpp"

namespace puffin {

    struct InputConfiguration {
        Window*     window;
        // Allocator* allocator;
    };

    // These match GLFW's mouse enum
    enum MouseButton {
        MOUSE_BUTTON_LAST = 0,
        MOUSE_BUTTON_LEFT = 1,
        MOUSE_BUTTON_RIGHT = 2,
        MOUSE_BUTTON_MIDDLE = 3,
        MOUSE_BUTTON_COUNT = 8
    };

    struct InputVector2 {
        f32         x;
        f32         y;
    };

    struct InputService : public Service {

    public:

        void init(InputConfiguration* config);
        void start_new_frame();

        bool is_key_down(Key key);
        bool is_key_just_pressed(Key key);
        bool is_key_just_released(Key key);

        bool is_mouse_button_down(MouseButton button);
        bool is_mouse_button_just_pressed(MouseButton button);
        bool is_mouse_button_just_released(MouseButton button);

        u8 keys[KEY_COUNT];
        u8 first_frame_keys[KEY_COUNT];
        u8 released_keys[KEY_COUNT];

        u8 mouse_buttons[MOUSE_BUTTON_COUNT];
        u8 first_frame_mouse_buttons[MOUSE_BUTTON_COUNT];
        u8 released_mouse_buttons[MOUSE_BUTTON_COUNT];

        InputVector2 mouse_position = { 0.0f, 0.0f };
        InputVector2 previous_mouse_position = { 0.0f, 0.0f };

        static constexpr cstring k_name     = "puffin_input_service";

        Window* window;
    };

}