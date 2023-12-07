//
// Created by darby on 12/6/2023.
//

#pragma once

#include "assert.hpp"
#include "platform.hpp"

namespace puffin {

// Math utils
// conversion from float/double to int/uint
//
// define this macro to check if converted value can be contained in the destination int/uint
#define PUFFIN_MATH_OVERFLOW_CHECK

#undef max
#undef min

template <typename T>
T                       max(const T& a, const T& b) { return a > b ? a : b; }

template <typename T>
T                       min(const T& a, const T& b) { return a < b ? a : b; }

template <typename T>
T                       clamp(const T& v, const T& a, const T& b) { return v < a ? a : (v > b ? b : v ); }

template <typename To, typename From>
To safe_case(From a) {
    To result = (To)a;

    From check = (From)result;
    PASSERT( check == result );

    return result;
}

u32                             ceilu32( f32 value );
u32                             ceilu32( f64 value );
u16                             ceilu16( f32 value );
u16                             ceilu16( f64 value );
i32                             ceili32( f32 value );
i32                             ceili32( f64 value );
i16                             ceili16( f32 value );
i16                             ceili16( f64 value );

u32                             flooru32( f32 value );
u32                             flooru32( f64 value );
u16                             flooru16( f32 value );
u16                             flooru16( f64 value );
i32                             floori32( f32 value );
i32                             floori32( f64 value );
i16                             floori16( f32 value );
i16                             floori16( f64 value );

u32                             roundu32( f32 value );
u32                             roundu32( f64 value );
u16                             roundu16( f32 value );
u16                             roundu16( f64 value );
i32                             roundi32( f32 value );
i32                             roundi32( f64 value );
i16                             roundi16( f32 value );
i16                             roundi16( f64 value );

f32 get_random_value( f32 min, f32 max );

const f32 puf_pi = 3.1415926538f;
const f32 puf_pi_2 = 1.57079632679f;



}

