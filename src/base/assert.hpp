//
// Created by darby on 5/10/2023.
//

#pragma once

#include "log.hpp"

namespace puffin {

    #define PASSERT(condition)      if(!(condition)) { p_print(PUFFIN_FILELINE("FALSE\n"));  }

}