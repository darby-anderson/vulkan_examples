//
// Created by darby on 5/10/2023.
//

#pragma once

#include "GLFW/glfw3.h"

#include "assert.hpp"
#include "platform.hpp"
#include "service.hpp"
#include "array.hpp"

namespace puffin {

    struct WindowConfiguration {
        u32             width;
        u32             height;

        cstring         name;
        Allocator*      allocator;
    };

    typedef void        (*OsMessagesCallback) (void* os_event, void* user_data);

    struct Window : public Service {
        void            init(void* configuration) override;
        void            shutdown() override;

        void            request_os_messages();

        void            set_fullscreen(bool value);

//        void            register_os_messages_callback(OsMessagesCallback callback, void* user_data);
//        void            deregister_os_messages_callback(OsMessagesCallback callback);

        void            center_mouse(bool dragging);

        bool            has_focus();

        bool            should_exit();



        void            set_window_user_pointer(void* user);
        void            set_key_press_callback(GLFWkeyfun callback);
        void            set_mouse_button_press_callback(GLFWmousebuttonfun callback);
        void            set_framebuffer_resize_callback(GLFWframebuffersizefun callback);


//        Array<OsMessagesCallback>   os_messages_callbacks;
//        Array<void*>                os_messages_callbacks_data;

        void*           platform_handle     = nullptr;
        // bool            request_exist       = false;
        bool            resized             = false;
        bool            minimized           = false;
        u32             width               = 0;
        u32             height              = 0;
        f32             display_refresh     = 1.0f / 60.0f;

        static constexpr cstring k_name     = "puffin_window_service";

    };


}
