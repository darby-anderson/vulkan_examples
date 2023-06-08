//
// Created by darby on 6/1/2023.
//

#pragma once

#include "memory.hpp"
#include "assert.hpp"
#include "bit.hpp"

#include "wyhash.h"

namespace puffin {

    // Hash Map ////
    static const u64        k_iterator_end = u64_max;

    struct FindInfo {
        u64                 offset;
        u64                 probe_length;
    };

    struct FindResult {
        u64                 index;
        bool                free_index; // States if the index is free or used.
    };

    struct FlatHashMapIterator {
        u64                 index;

        bool                is_valid() const { return index != k_iterator_end; }
        bool                is_invalid() const { return index == k_iterator_end; }
    };

    // A single block of empty control bytes for tables without any slots allocated
    // This enables removing a branch in the hot path of find()
    i8*                     group_init_empty();


    // Probing //////////
    struct ProbeSequence {
        static const u64    k_width = 16;
        static const size_t k_engine_hash = 0x31d3a36013e;

        ProbeSequence(u64 hash, u64 mask);

        // 0-based probe index. The i-th probe in the probe sequence
        u64                 get_index() const;

        void                next();

        u64                 mask;
        u64                 offset;
        u64                 index = 0;
    };

    template <typename K, typename V>
    struct FlatHashMap {

        struct KeyValue {
            K               key;
            V               value;
        };

        void                init(Allocator* allocator, u64 initial_capacity);
        void                shutdown();

        // Main interface
        FlatHashMapIterator find(const K& key);
        void                insert(const K& key, const V& value);
        u32                 remove(const K& key);
        u32                 remove(const FlatHashMapIterator& it);

        V&                  get(const K& key);
        V&                  get(const FlatHashMapIterator& it);

        KeyValue&

    };


}
