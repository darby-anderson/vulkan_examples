//
// Created by darby on 6/1/2023.
//

#include "bit.hpp"
#include "log.hpp"
#include "memory.hpp"

#if defined (_MSC_VER)
#include <immintrin.h>
#include <intrin0.h>
#endif
#include <string.h>

namespace puffin {

    u32 leading_zeroes_u32(u32 x) {
#if defined(_MSC_VER)
        return _lzcnt_u32(x);
#else
        return __builtin_clz(x);
#endif
    }



#if defined(_MSC_VER)
    u32 leading_zeroes_u32_msvc(u32 x) {
        unsigned long result = 0;
        if ( _BitScanReverse( &result, x ) ) {
            return 31 - result;
        }
        return 32;
    }
#endif

    u32 trailing_zeroes_u32(u32 x) {
#if defined(_MSC_VER)
        return _tzcnt_u32(x); // trailing zero count
#else
        return __builtin_ctz(x);
#endif
    }

    u64 trailing_zeroes_u64(u64 x) {
#if defined(_MSC_VER)
        return _tzcnt_u64(x);
#else
        return __builtin_ctzl(x);
#endif
    }

    u32 round_up_to_power_of_2(u32 v) {
        u32 nv = 1 << (32 - leading_zeroes_u32(v));
        return nv;
    }

    void print_binary(u64 n) {
        p_print("0b");
        for(u32 i = 0; i < 64; i++) {
            u64 bit = (n >> (64 - i - 1)) & 0x1;
            p_print("%llu", bit);
        }
        p_print(" ");
    }

    void print_binary(u32 n) {
        p_print("0b");
        for(u32 i = 0; i < 32; i++) {
            u64 bit = (n >> (32 - i - 1)) & 0x1;
            p_print("%u", bit);
        }
        p_print(" ");
    }


}