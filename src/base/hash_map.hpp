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

        u64                 get_offset() const;
        u64                 get_offset(u64 i) const;

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

        KeyValue&           get_structure(const K& key);
        KeyValue&           get_structure(const FlatHashMapIterator& it);

        void                set_default_value(const V& value);

        // Iterators
        FlatHashMapIterator iterator_begin();
        void                iterator_advance(FlatHashMapIterator& it);

        void                clear();
        void                reserve(u64 new_size);

        // Internal Methods
        void                erase_meta(const FlatHashMapIterator& it);

        FindResult          find_or_prepare_insert(const K& key);
        FindInfo            find_first_non_full(u64 hash);

        u64                 prepare_insert(u64 hash);

        ProbeSequence       probe(u64 hash);
        void                rehash_and_grow_if_necessary();

        void                drop_deletes_without_resize();
        u64                 calculate_size(u64 new_capacity);

        void                initialize_slots();

        void                resize(u64 new_capacity);

        void                iterator_skip_empty_or_deleted(FlatHashMapIterator& iterator);

        // Sets the control byte, and if 'i < Group::kWidth - 1" set the cloned byte at the end too.
        void                set_ctrl(u64 i, i8 h);
        void                reset_ctrl();
        void                reset_growth_remaining();

        // Manages the state of slots in the hash map
        i8*                 control_bytes   = group_init_empty();
        KeyValue*           slots_          = nullptr;

        u64                 size            = 0;    // Occupied size
        u64                 capacity        = 0;    // Allocated capacity
        u64                 growth_remaining     = 0;    // Number of empty spaces we can fill

        Allocator*          allocator       = nullptr;
        KeyValue            default_key_value = {(K)-1, 0};

    }; // struct FlatHashMap

    // Implementation
    template<typename T>
    inline u64 hash_calculate(const T& value, size_t seed = 0) {
        return wyhash(&value, sizeof(T), seed, _wyp);
    }

    template<size_t N>
    inline u64 hash_calculate(const char (&value)[N], size_t seed = 0) {
        return wyhash(value, strlen(value), seed, _wyp);
    }

    template<>
    inline u64 hash_calculate(const cstring& value, size_t seed) {
        return wyhash(value, strlen(value), seed, _wyp);
    }

    // Method to hash memory itself
    inline u64 hash_bytes(void* data, size_t length, size_t seed = 0) {
        return wyhash(data, length, seed, _wyp);
    }

    // https://gankra.github.io/blah/hashbrown-tldr/
    // https://blog.waffles.space/2018/12/07/deep-dive-into-hashbrown/
    // https://abseil.io/blog/20180927-swisstables
    //

    // Control Bytes
    // Follows google's abseil library convention - based on performance
    static const i8     k_control_bitmask_empty     = -128; // 0b1000-0000
    static const i8     k_control_bitmask_deleted   = -2;   // 0b1111-1110
    static const i8     k_control_bitmask_sentinel  = -1;   // 0b1111-1111

    static bool         control_is_empty(i8 control)        { return control == k_control_bitmask_empty; }
    static bool         control_is_full(i8 control)         { return control >= 0; }
    static bool         control_is_deleted(i8 control)      { return control == k_control_bitmask_deleted; }
    static bool         control_is_empty_or_deleted(i8 control) { return control < k_control_bitmask_sentinel; }

    // Returns a hash seed.
    //
    // The seed consists of the ctrl_ pointer, which adds enough entropy to ensure
    // non-determinism of iteration order in most cases.
    // Implementation details: the low bits of the pointer have little or no entropy because
    // of alignment. We shift the pointer to attempt to access bits with higher entropy.
    // A good number seems to be 12-bits, as that aligns with page size
    static u64          hash_seed(const i8* control)    { return reinterpret_cast<uintptr_t>(control) >> 12; }

    static u64          hash_1(u64 hash, const i8* ctrl) { return (hash >> 7) ^ hash_seed(ctrl); }
    static i8           hash_2(u64 hash)                { return hash & 0x7F; }

    struct GroupSse2Impl {
        static constexpr size_t kWidth = 16; // # of slots per group

        __m128i ctrl;

        explicit GroupSse2Impl(const i8* pos) {
            // _mm_loadu_si128
            // Loads a 128-bit value from the memory address specified by pos
            // Loads the ctrl bits in here
            ctrl = _mm_loadu_si128(reinterpret_cast< const __m128i* > (pos));
        }

        // Returns a bitmask representing the positions of empty slots
        BitMask<uint32_t, kWidth> Match(i8 hash) const {
            // Sets all elements of a 128-bit register to the specified 8-byte value
            __m128i match = _mm_set1_epi8(hash);

            // _mm_cmpeq_epi8
            // compares two __m128i registers and sets the bits to 0xFF in the result
            // register if they are the same, and 0x00 if not
            // Used here to check if the ctrl bits are equal to the hash bits
            __m128i comparisonResult = _mm_cmpeq_epi8(match, ctrl);

            // _mm_movemask_epi8
            // extracts the most significant bit of each byte in a __m128i reg and packs them into a 16-bit mask
            // Used here to generate a bitmask representing the matching positions of slots or the positions
            // of matching slots
            uint32_t matchingByteMask = _mm_movemask_epi8(comparisonResult);

            return BitMask<uint32_t, kWidth> (
                    matchingByteMask
                    );
        }

        // Returns a bitmask representing the position of empty slots
        BitMask<uint32_t, kWidth> MatchEmpty() const {
            return Match(static_cast<i8> (k_control_bitmask_empty));
        }

        // Returns a bitmask representing the positions of empty or deleted slots
        BitMask<uint32_t, kWidth> MatchEmptyOrDeleted() const {
            // set all sets of 8 bits to the sentinel value
            __m128i special = _mm_set1_epi8(k_control_bitmask_sentinel);

            // compares two __m128i registers and sets the bits to 0xFF in the result
            // register if the first argument is greater than the second, and 0x00 if not
            __m128i comparisonResult = _mm_cmpgt_epi8(special, ctrl);

            // Checks if each of the bytes in comparisonResult are TRUE, creating a mask out of it
            uint32_t result = _mm_movemask_epi8( comparisonResult);

            return BitMask<uint32_t, kWidth>(
                         result
                    );
        }

        // Returns the number of trailing empty or deleted elements in the group
        uint32_t CountLeadingEmptyOrDeleted() const {
            auto special = _mm_set1_epi8(k_control_bitmask_sentinel);
            return trailing_zeroes_u32(static_cast<uint32_t>(
                    _mm_movemask_epi8(_mm_cmpgt_epi8(special, ctrl)) + 1
                    ));
        }

        void ConvertSpecialToEmptyAndFullToDeleted( i8* dst ) {
            auto msbs = _mm_set1_epi8(static_cast<char>(-128));
            auto x126 = _mm_set1_epi8(126); // 0b0111 1110
            auto zero = _mm_setzero_si128();
            auto special_mask = _mm_cmpgt_epi8(zero, ctrl);
            // bitwise or, & ~, so msbs | (special_mask & ~x126)
            auto res = _mm_or_si128(msbs, _mm_andnot_si128(special_mask, x126));

            // Stores __m128i data into a pointer
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), res);
        }
    };

    static bool         capacity_is_valid(size_t n);

    // Rounds up capacity to the next power of 2 minus 1, with the minimum of 1
    static u64          capacity_normalize(u64 n);

    // General notes on capacity/growth methods below:
    // - We use 7/8th as maximum load factor. For 16-wide groups, that gives an
    //   average of two empty slots per group.
    // - For (capacity+1) >= Group::kWidth, growth 7/8*capacity
    // - For (capacity+1) < Group::kWidth, growth == capacity. In this case, we
    //   never need to probe (the whole table fits in one group) so we don't need a
    //   load factor less than 1

    // Give `capacity` of the table, returns the size (i.e. number of full slots)
    // at which we should grow the capacity.
    // if (Group::kWidth == 8 && capacity == 7) { return 6; }
    // x-x/8 does not work when x==7
    static u64          capacity_to_growth(u64 capacity);
    static u64          capacity_growth_to_lower_bound(u64 growth);

    static void ConvertDeletedToEmptyAndFullToDeleted(i8* ctrl, size_t capacity) {
        for(i8* pos = ctrl; pos != ctrl + capacity + 1; pos += GroupSse2Impl::kWidth) {
            GroupSse2Impl{pos}.ConvertSpecialToEmptyAndFullToDeleted(pos);
        }

        // Copy the cloned ctrl bytes
        puffin::memory_copy(ctrl + capacity + 1, ctrl, GroupSse2Impl::kWidth);
        ctrl[capacity] = k_control_bitmask_sentinel;
    }

    // FlatHashMap
    template <typename K, typename V>
    void FlatHashMap<K, V>::reset_ctrl() {
        memset(control_bytes, k_control_bitmask_empty, capacity + GroupSse2Impl::kWidth);
        control_bytes[capacity] = k_control_bitmask_sentinel;
    }

    template <typename K, typename V>
    void FlatHashMap<K, V>::reset_growth_remaining() {
        growth_remaining = capacity_to_growth(capacity) - size;
    }

    template <typename K, typename V>
    ProbeSequence FlatHashMap<K, V>::probe(u64 hash) {
        return ProbeSequence(hash_1(hash, control_bytes), capacity);
    }

    template <typename K, typename V>
    inline void FlatHashMap<K, V>::init(Allocator* allocator_, u64 initial_capacity) {
        allocator = allocator_;
        size = capacity = growth_remaining = 0;
        default_key_value = { (K)-1, (V)0 };

        control_bytes = group_init_empty();
        slots_ = nullptr;
        reserve(initial_capacity < 4 ? 4 : initial_capacity);
    }

    template <typename K, typename V>
    inline void FlatHashMap<K, V>::shutdown() {
        puffin_free(control_bytes, allocator);
    }

    template<typename K, typename V>
    FlatHashMapIterator FlatHashMap<K, V>::find(const K& key) {
        const u64 hash = hash_calculate(key);
        ProbeSequence sequence = probe(hash);

        while(true) {
            const GroupSse2Impl group { control_bytes + sequence.get_offset() };
            const i8 hash2 = hash_2(hash);
            for(int i : group.Match(hash2)) {
                const KeyValue& key_value = *(slots_ + sequence.get_offset(i));
                if(key_value.key == key) {
                    return {sequence.get_offset(i)};
                }
            }

            if(group.MatchEmpty()) {
                break;
            }

            sequence.next();
        }
        return { k_iterator_end };
    }

    template<typename K, typename V>
    void FlatHashMap<K, V>::insert(const K& key, const V& value) {
        const FindResult find_result = find_or_prepare_insert(key);
        if(find_result.free_index) {
            // emplace
            slots_[find_result.index].key = key;
            slots_[find_result.index].value = value;
        } else {
            // substitute value
            slots_[find_result.index].value = value;
        }
    }

    template<typename K, typename V>
    void FlatHashMap<K, V>::erase_meta(const FlatHashMapIterator& iterator) {
        --size;

        const u64 index = iterator.index;
        const u64 index_before = (index - GroupSse2Impl::kWidth) & capacity;
        const auto empty_after = GroupSse2Impl(control_bytes + index).MatchEmpty();
        const auto empty_before = GroupSse2Impl(control_bytes + index_before).MatchEmpty();

        // We count how many consecutive non-empties we have to the right and to the
        // left of `it`. If the sum is >= kWidth then there is at least one probe
        // window that might have seen a full group
        const u64 trailing_zeros = empty_after.TrailingZeros();
        const u64 leading_zeros = empty_before.LeadingZeros();
        const u64 zeros = trailing_zeros + leading_zeros;

        // Set the control bits
        bool was_never_full = empty_before && empty_after;
        was_never_full = was_never_full && (zeros < GroupSse2Impl::kWidth);

        set_ctrl(index, was_never_full ? k_control_bitmask_empty : k_control_bitmask_deleted);
        growth_remaining += was_never_full;
    }

    template <typename K, typename V>
    u32 FlatHashMap<K, V>::remove(const K& key) {
        FlatHashMapIterator iterator = find(key);
        if(iterator.index == k_iterator_end){
            return 0;
        }

        erase_meta(iterator);
        return 1;
    }

    template <typename K, typename V>
    u32 FlatHashMap<K, V>::remove(const FlatHashMapIterator& iterator) {
        if(iterator.index == k_iterator_end) {
            return 0;
        }

        erase_meta(iterator);
        return 1;
    }

    template <typename K, typename V>
    FindResult FlatHashMap<K, V>::find_or_prepare_insert(const K& key) {
        u64 hash = hash_calculate(key);
        ProbeSequence sequence = probe(hash);

        while(true) {
            const GroupSse2Impl group {control_bytes + sequence.get_offset() };
            for(int i : group.Match(hash_2(hash))) {
                const KeyValue& key_value = *(slots_ + sequence.get_offset(i));
                if(key_value.key == key) {
                    return { sequence.get_offset(i), false };
                }
            }
            if(group.MatchEmpty()) {
                break;
            }
            sequence.next();
        }
        return { prepare_insert(hash), true };
    }

    template <typename K, typename V>
    FindInfo FlatHashMap<K, V>::find_first_non_full(u64 hash) {
        ProbeSequence sequence = probe(hash);

        while(true) {
            const GroupSse2Impl group {control_bytes + sequence.get_offset()};
            auto mask = group.MatchEmptyOrDeleted();

            if(mask) {
                return {sequence.get_offset(mask.LowestBitSet()), sequence.get_index()};
            }

            sequence.next();
        }

        return FindInfo();
    }

    template <typename K, typename V>
    u64 FlatHashMap<K, V>::prepare_insert(u64 hash) {
        FindInfo find_info = find_first_non_full(hash);
        if(growth_remaining == 0 && !control_is_deleted(control_bytes[find_info.offset])) {
            rehash_and_grow_if_necessary();
            find_info = find_first_non_full(hash);
        }
        size++;

        growth_remaining -= control_is_empty(control_bytes[find_info.offset]) ? 1 : 0;
        set_ctrl(find_info.offset, hash_2(hash));
        return find_info.offset;
    }

    template <typename K, typename V>
    void FlatHashMap<K, V>::rehash_and_grow_if_necessary() {
        if(capacity == 0) {
            resize(1);
        } else if(size <= capacity_to_growth(capacity) / 2) {
            // Squash DELETED without growing if there is enough capacity.
            drop_deletes_without_resize();
        } else {
            // Otherwise grow the container
            resize(capacity * 2 + 1);
        }
    }

    template <typename K, typename V>
    void FlatHashMap<K, V>::drop_deletes_without_resize() {
        // Algorithm
        //  - mark all DELETED slots as empty
        //  - mark all FULL slots as DELETED
        //  - for each slot marked as DELETED
        //      hash = Hash(element)
        //      target = find_first_non_full(hash)
        //      if target is in the same group
        //          mark slot has FULL
        //      else if target is EMPTY
        //          transfer element to target
        //          mark slot as EMPTY
        //          mark target as FULL
        //      else if target is DELETED
        //          swap current element with target element
        //          mark target as FULL
        //          repeat procedure for current slot w/ moved from element (target)
        //

        alignas(KeyValue) unsigned char raw[sizeof(KeyValue)];
        size_t total_probe_length = 0;
        KeyValue* slot = reinterpret_cast<KeyValue*>(&raw);
        for(size_t i = 0; i != capacity; i++) {
            if(!control_is_deleted(control_bytes[i])) {
                continue;
            }

            const KeyValue* current_slot = slots_ + i;
            size_t hash = hash_calculate(current_slot->key);
            auto target = find_first_non_full(hash);
            size_t new_i = target.offset;
            total_probe_length += target.probe_length;

            // Verify if the old and new i fall within the same group wrt the hash
            // If they do, we don't need to move the ojbect as it falls already in the best probe
            const auto probe_index = [&](size_t pos) {
                return ((pos - probe(hash).get_offset()) & capacity) / GroupSse2Impl::kWidth;
            };

            // Element doesn't move
            if((probe_index(new_i) == probe_index(i))) {
                set_ctrl(i, hash_2(hash));
                continue;
            }

            if(control_is_empty(control_bytes[new_i])) {
                // Transfer element to the empty slot
                // set_ctrl poisons/unpoisons the slots so we have to call it at the right time
                set_ctrl(new_i, hash_2(hash));
                memcpy(slots_ + new_i, slots_ + i, sizeof(KeyValue));
                set_ctrl(i, k_control_bitmask_empty);
            } else {
                set_ctrl(new_i, hash_2(hash));
                // Until we are done rehashing, DELETED marks previously FULL slots.
                // Swap i and new_i elements
                memcpy(slot, slots_ + i, sizeof(KeyValue));
                memcpy(slots_ + i, slots_ + new_i, sizeof(KeyValue));
                memcpy(slots_ + new_i, slot, sizeof(KeyValue));
                i--;
            }
        }

        reset_growth_remaining();
    }

    template <typename K, typename V>
    u64 FlatHashMap<K, V>::calculate_size(u64 new_capacity) {
        return (new_capacity + GroupSse2Impl::kWidth + new_capacity * (sizeof(KeyValue)));
    }

    template <typename K, typename V>
    void FlatHashMap<K, V>::initialize_slots() {
        char* new_memory = (char*) puffin_alloc(calculate_size(capacity), allocator);

        control_bytes = reinterpret_cast<i8*>(new_memory);
        slots_ = reinterpret_cast<KeyValue*>(new_memory + capacity + GroupSse2Impl::kWidth);

        reset_ctrl();
        reset_growth_remaining();
    }

    template <typename K, typename V>
    void FlatHashMap<K, V>::resize(u64 new_capacity) {
        i8* old_control_bytes = control_bytes;
        KeyValue* old_slots = slots_;
        const u64 old_capacity = capacity;

        capacity = new_capacity;

        initialize_slots();

        size_t total_probe_length = 0;
        for(size_t i = 0; i != old_capacity; i++) {
            if(control_is_full(old_control_bytes[i])) {
                const KeyValue* old_value = old_slots + i;
                u64 hash = hash_calculate(old_value->key);

                FindInfo find_info = find_first_non_full(hash);

                u64 new_i = find_info.offset;
                total_probe_length += find_info.probe_length;

                set_ctrl(new_i, hash_2(hash));

                puffin::memory_copy(slots_ + new_i, old_slots + i, sizeof(KeyValue));
            }
        }

        if(old_capacity) {
            puffin_free(old_control_bytes, allocator);
        }
    }

    // Sets the control bytes, and if 'i <Group::kWdith-1>', set the cloned byte
    // at the end too
    template<typename K, typename V>
    void FlatHashMap<K, V>::set_ctrl(u64 i, i8 h) {
        control_bytes[i] = h;
        constexpr size_t kClonedBytes = GroupSse2Impl::kWidth - 1;
        control_bytes[((i - kClonedBytes) & capacity) + (kClonedBytes & capacity)] = h;
    }

    template <typename K, typename V>
    V& FlatHashMap<K, V>::get(const K& key) {
        FlatHashMapIterator iterator = find(key);
        if(iterator.index != k_iterator_end) {
            return slots_[iterator.index].value;
        }
        return default_key_value.value;
    }

    template<typename K, typename V>
    V& FlatHashMap<K, V>::get(const FlatHashMapIterator& iterator) {
        if(iterator.index != k_iterator_end) {
            return slots_[iterator.index].value;
        }
        return default_key_value.value;
    }

    template <typename K, typename V>
    typename FlatHashMap<K, V>::KeyValue& FlatHashMap<K, V>::get_structure(const K& key) {
        FlatHashMapIterator iterator = find(key);
        if(iterator.index != k_iterator_end) {
            return slots_[iterator.index];
        }
        return default_key_value;
    }

    template <typename K, typename V>
    typename FlatHashMap<K, V>::KeyValue& FlatHashMap<K,V>::get_structure(const FlatHashMapIterator& iterator) {
        return slots_[iterator.index];
    }

    template<typename K, typename V>
    inline void FlatHashMap<K, V>::set_default_value(const V& value) {
        default_key_value.value = value;
    }

    template<typename K, typename V>
    FlatHashMapIterator FlatHashMap<K, V>::iterator_begin() {
        FlatHashMapIterator it{0};
        iterator_skip_empty_or_deleted(it);
        return it;
    }

    template<typename K, typename V>
    void FlatHashMap<K, V>::iterator_advance(FlatHashMapIterator& iterator) {
        iterator.index++;
        iterator_skip_empty_or_deleted(iterator);
    }

    template<typename K, typename V>
    inline void FlatHashMap<K, V>::iterator_skip_empty_or_deleted(FlatHashMapIterator& iterator) {
        i8* ctrl = control_bytes + iterator.index;

        while(control_is_empty_or_deleted(*ctrl)) {
            u32 shift = GroupSse2Impl{ctrl}.CountLeadingEmptyOrDeleted();
            ctrl += shift;
            iterator.index += shift;
        }
        if(*ctrl == k_control_bitmask_sentinel) {
            iterator.index = k_iterator_end;
        }
    }

    template<typename K, typename V>
    inline void FlatHashMap<K, V>::clear() {
        size = 0;
        reset_ctrl();
        reset_growth_remaining();
    }

    template<typename K, typename V>
    inline void FlatHashMap<K, V>::reserve(u64 new_size) {
        if(new_size > size + growth_remaining) {
            size_t m = capacity_growth_to_lower_bound(new_size);
            resize(capacity_normalize(m));
        }
    }

    // Capacity ///

    // Checks n is a power of 2 minus 1
    // power of 2 minus 1 size is helpful with wraparound
    bool capacity_is_valid(size_t n)        { return ((n + 1) & n) == 0 && n > 0; }

    inline u64 lzcnt_soft(u64 n) {
        unsigned long index = 0;
        _BitScanReverse64(&index, n);
        u64 cnt = index ^ 63;
        return cnt;
    }

    // Rounds up capacity to the next power of 2 minus 1, with a minimum of 1
    u64 capacity_normalize ( u64 n )        { return n ? ~u64{} >> lzcnt_soft(n) : 1; }

    u64 capacity_to_growth (u64 capacity)   { return capacity - capacity / 8; }



    u64 capacity_growth_to_lower_bound(u64 growth) {
        return growth + static_cast<u64>((static_cast<i64>(growth) - 1) / 7);
    }

    // Grouping: implementation /////////
    inline i8* group_init_empty() {
        alignas(16) static constexpr i8 empty_group[] = {
                k_control_bitmask_sentinel, k_control_bitmask_empty,
                k_control_bitmask_empty, k_control_bitmask_empty,
                k_control_bitmask_empty, k_control_bitmask_empty,
                k_control_bitmask_empty, k_control_bitmask_empty,
                k_control_bitmask_empty, k_control_bitmask_empty,
                k_control_bitmask_empty, k_control_bitmask_empty,
                k_control_bitmask_empty, k_control_bitmask_empty,
                k_control_bitmask_empty, k_control_bitmask_empty,
        };
        return const_cast<i8*>(empty_group);
    }

    // Probing: implementation ///////
    inline ProbeSequence::ProbeSequence(u64 hash_, u64 mask_) {
        mask = mask_;
        offset = hash_ & mask_;
    }

    inline u64 ProbeSequence::get_offset() const {
        return offset;
    }

    inline u64 ProbeSequence::get_offset(u64 i) const {
        return (offset + i) & mask;
    }

    inline u64 ProbeSequence::get_index() const {
        return index;
    }

    inline void ProbeSequence::next() {
        index += k_width;
        offset += index;
        offset &= mask;
    }

}
