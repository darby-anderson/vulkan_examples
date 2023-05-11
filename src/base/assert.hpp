//
// Created by darby on 5/10/2023.
//

#pragma once

#include "log.hpp"

namespace puffin {

    #define PASSERT(condition)      if(!(condition)) { rprint(PUFFIN_FILELINE("FALSE\n"));  }

}