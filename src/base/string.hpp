//
// Created by darby on 6/1/2023.
//

#pragma once

#include "platform.hpp"

namespace puffin {
    // Forward Declarations
    struct Allocator;

    template <typename K, typename V>
    struct FlatHashMap;

    struct FlatHashMapIterator;

    ///////////////////////

    struct StringView {
        char*           text;
        size_t          length;

        static bool     equals(const StringView& a, const StringView& b);
        static void     copy_to(const StringView& a, char* buffer, size_t buffer_size);
    };


    struct StringBuffer {
        void            init(Allocator* allocator, size_t size);
        void            shutdown();

        void            append(cstring string);
        void            append(const StringView& view);
        void            append_m(void* memory, size_t size);
        void            append(const StringBuffer& other_buffer);
        void            append_f(cstring format, ... );

        char*           append_use(cstring string);
        char*           append_use_f(cstring format, ...);
        char*           append_use(const StringView& view);
        char*           append_use_substring(cstring string, u32 start_index, u32 end_index);

        void            close_current_string();

        // Index interface
        u32             get_index(cstring text) const;
        cstring         get_text(u32 index) const;

        char*           reserve(size_t size);

        char*           current() { return data + current_size; }

        void            clear();

        char*           data            = nullptr;
        u32             buffer_size     = 1024;
        u32             current_size    = 0;
        Allocator*      allocator       = nullptr;
    };

    struct StringArray {

        void                    init(Allocator* allocator, size_t size);
        void                    shutdown();
        void                    clear();

        FlatHashMapIterator*    begin_string_iteration();
        size_t                  get_string_count() const;
        cstring                 get_string(u32 index) const;
        cstring                 get_next_string(FlatHashMapIterator* it) const;
        bool                    has_next_string(FlatHashMapIterator* it) const;

        cstring                 intern(cstring string);

        FlatHashMap<u64, u32>*  string_to_index; // Attempt to avoid include the hash map header
        FlatHashMapIterator*    strings_iterator;

        char*                   data            = nullptr;
        u32                     buffer_size     = 1024;
        u32                     current_size    = 0;

        Allocator*              allocator       = nullptr;
    };

}
