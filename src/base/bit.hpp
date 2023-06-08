//
// Created by darby on 6/1/2023.
//

#pragma once

#include "platform.hpp"

namespace puffin {
    struct Allocator;

    // Common bit methods ////////////////
    u32             leading_zeroes_u32(u32 x);
#if defined (_MSC_VER)
    u32             leading_zeroes_u32_msvc(u32 x);
#endif
    u32             trailing_zeroes_u32(u32 x);
    u64             trailing_zeroes_u64(u64 x);

    u32             round_up_to_power_of_2(u32 v);

    void            print_binary(u64 n);
    void            print_binary(u32 n);

}