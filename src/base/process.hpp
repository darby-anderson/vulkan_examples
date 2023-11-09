//
// Created by darby on 8/9/2023.
//
#pragma once

#include "platform.hpp"

namespace puffin {
    bool                process_execute(cstring working_directory, cstring process_fullpath,
                                        cstring arguments, cstring search_error_string = "");

    cstring             process_get_output();
}