//
// Created by darby on 5/10/2023.
//

#pragma once

#include "service.hpp"
#include "platform.hpp"

namespace puffin {
    typedef void                    (*PrintCallback)(cstring);

    struct LogService : public Service {
        PUFFIN_DECLARE_SERVICE(LogService);

        void                        print_format(cstring format, ...);

        void                        set_callback(PrintCallback callback);

        PrintCallback               print_callback  = nullptr;

        static constexpr cstring    k_name          = "puffin_log_service";
    };

    #define rprint(format, ...)     puffin::LogService::instance()->print_format(format, __VA_ARGS__);
    #define rprintret(format, ...)  puffin::LogService::instance()->print_format(format, __VA_ARGS__); puffin::LogService::instance()->print_format("\n");
}