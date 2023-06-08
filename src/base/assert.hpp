//
// Created by darby on 5/10/2023.
//

#pragma once

#include "log.hpp"

namespace puffin {

    #define PASSERT(condition)      if(!(condition)) { p_print(PUFFIN_FILELINE("FALSE\n"));  }

#if defined (_MSC_VER)
    #define PASSERTM( condition, message, ... ) if (!(condition)) { p_print(PUFFIN_FILELINE(PUFFIN_CONCAT(message, "\n")), __VA_ARGS__); PUFFIN_DEBUG_BREAK }
#else
    #define PASSERTM( condition, message, ... ) if (!(condition)) { p_print(PUFFIN_FILELINE(PUFFIN_CONCAT(message, "\n")), ## __VA_ARGS__); PUFFIN_DEBUG_BREAK }
#endif


}