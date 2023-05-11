//
// Created by darby on 5/10/2023.
//
#include "log.hpp"

#include <stdio.h>
#include <stdarg.h>

namespace puffin {

    LogService              s_log_service;
    static constexpr u32    k_string_buffer_size = 1024 * 1024;
    static char             log_buffer[ k_string_buffer_size ];

    static void output_console(char* log_buffer_){
        printf("%s", log_buffer_);
    }

    LogService* LogService::instance() {
        return &s_log_service;
    }

    void LogService::print_format(cstring format, ...) {
        va_list args;

        va_start(args, format);
        vsnprintf(log_buffer, PuffinArraySize(log_buffer), format, args);
        log_buffer[PuffinArraySize(log_buffer) - 1] = '\0';
        va_end(args);

        output_console(log_buffer);

        if(print_callback){
            print_callback(log_buffer);
        }
    }

    void LogService::set_callback(PrintCallback callback) {
        print_callback = callback;
    }
}


