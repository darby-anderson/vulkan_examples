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

    // class BitMask

    // An abstractino over a bitmask. It provides and easy way to iterate
    // through the indices of the set bits of a bitmask. When Shift=0 (SSE platforms)
    // this is a true bitmask. On non-SSE platforms the arithmetic used to emulate the SSE
    // behavior works in bytes (Shift=3) and leaves each byte as either 0x00 or 0x80
    //
    // E.G.
    // for (int i : BitMask<uint32_t, 16>(0x5)) -> yields 0, 2
    // for (int i : BitMask<uint64_t, 8, 3>(0x0000000080800000)) -> yields 2, 3

    template<class T, int SignificantBits, int Shift = 0>
    class BitMask {
    public:
        T mask_;

        explicit BitMask(T mask) : mask_(mask) { }

        // Allows for iteration through the bitmask, clearing the least significant bit each time
        BitMask& operator++() {
            mask_ &= (mask_ - 1);
            return *this;
        }

        explicit operator bool() const {
            return mask_ != 0;
        }

        // returns the lowest bit set
        int operator*() const {
            return LowestBitSet();
        }

        uint32_t LowestBitSet() const {
            return trailing_zeroes_u32(mask_) >> Shift;
        }

        uint32_t HighestBitSet() const {
            return static_cast<uint32_t>((bit_width(mask_) - 1) >> Shift);
        }

        BitMask begin() const {
            return *this;
        }

        BitMask end() const {
            return BitMask(0);
        }

        uint32_t TrailingZeros() const {
            return trailing_zeroes_u32(mask_); // >> Shift
        }

        uint32_t LeadingZeros() const {
            return leading_zeroes_u32(mask_); // >> Shift
        }

    private:
        // marked as friend to access the private member mask_
        // == operators aren't part of the class
        friend bool operator==(const BitMask& a, const BitMask& b) {
            return a.mask_ == b.mask_;
        }

        friend bool operator!=(const BitMask& a, const BitMask& b) {
            return a.mask_ != b.mask_;
        }
    };

    // Utility methods
    // Gets the bitmask for a specific index from the specified bit position in an 8-bit system
    inline u32          bit_mask_8(u32 bit) { return 1 << (bit & 7); }
    // Gets the slot index for a specific bit position
    inline u32          bit_slot_8(u32 bit) { return bit / 8; }

    struct BitSet {

        Allocator*      allocator = nullptr;
        u8*             bits = nullptr;
        u32             size = 0;

        void            init(Allocator* allocator, u32 total_bits);
        void            shutdown();

        void            resize(u32 total_bits);

        void            set_bit(u32 index) { bits[index / 8] |= bit_mask_8(index); }
        void            clear_bit(u32 index) { bits[index / 8] &= ~bit_mask_8((index)); }
        u8              get_bit(u32 index) { return bits[index / 8] & bit_mask_8(index); }
    };

    template <u32 SizeInBytes>
    struct BitSetFixed {

        u8              bits[ SizeInBytes ];

        void            set_bit(u32 index) { bits[index / 8] |= bit_mask_8(index); }
        void            clear_bit(u32 index) { bits[index / 8] &= ~bit_mask_8(index); }
        void            get_bit(u32 index) { bits[index / 8] & bit_mask_8(index); }
    };


}