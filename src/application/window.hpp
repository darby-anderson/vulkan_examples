//
// Created by darby on 5/10/2023.
//

#pragma once

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

    struct Window: public Service {
        void            init(void* configuration) override;
        void            shutdown() override;

        void            handle_os_messages();

        void            set_fullscreen(bool value);

        void            register_os_messages_callback(OsMessagesCallback callback, void* user_data);
        void            deregister_os_messages_callback(OsMessagesCallback callback);

        void            center_mouse(bool dragging);

          


    };


}
