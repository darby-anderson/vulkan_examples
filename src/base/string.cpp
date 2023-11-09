//
// Created by darby on 6/1/2023.
//

#include "string.hpp"
#include "memory.hpp"
#include "log.hpp"
#include "assert.hpp"

#include <stdio.h>
#include <stdarg.h>
#include <memory.h>

#include "hash_map.hpp"

#define PASSERT_OVERFLOW() PASSERT(false)

namespace puffin {

    // String View ////
    bool StringView::equals(const StringView& a, const StringView& b) {
        if(a.length != b.length) {
            return false;
        }

        for(u32 i = 0; i < a.length; i++) {
            if(a.text[i] != b.text[i]) {
                return false;
            }
        }

        return true;
    }
    void StringView::copy_to(const StringView& a, char* buffer, size_t buffer_size) {
        // account for null vector
        const size_t max_length = buffer_size - 1 < a.length ? buffer_size - 1 : a.length;
        memory_copy(buffer, a.text, max_length);
        buffer[a.length] = 0;
    }

    // StringBuffer /////
    void StringBuffer::init(Allocator* allocator_, size_t size) {
        if(data) {
            allocator->deallocate(data);
        }

        if(size < 1) {
            p_print("ERROR: Buffer cannot be empty!\n");
            return;
        }

        allocator = allocator_;
        data = (char*) puffin_alloc_return_mem_pointer(size + 1, allocator_);
        PASSERT(data);
        data[0] = 0;
        buffer_size = (u32) size;
        current_size = 0;
    }

    void StringBuffer::shutdown() {
        puffin_free(data, allocator);

        buffer_size = current_size = 0;
    }

    void StringBuffer::append(const char* string) {
        append_f("%s", string);
    }

    void StringBuffer::append_f(const char* format, ...) {
        if(current_size >= buffer_size) {
            PASSERT_OVERFLOW();
            p_print("Buffer full! please allocate more space");
            return;
        }

        va_list args;
        va_start(args, format);

        // MSC_VER
        int written_chars = vsnprintf_s(&data[current_size], buffer_size - current_size, _TRUNCATE, format, args);

        current_size += written_chars > 0 ? written_chars : 0;
        va_end(args);

        if(written_chars < 0) {
            PASSERT_OVERFLOW();
            p_print("New string too big for current buffer. String needs to allocate more size");
        }
    }

    void StringBuffer::append(const StringView& text) {
        const size_t max_length = current_size + text.length < buffer_size ? text.length : buffer_size - current_size;

        if(max_length == 0 | max_length >= buffer_size) {
            PASSERT_OVERFLOW();
            p_print("String buffer full. Allocate more space.\n");
            return;
        }

        memcpy(&data[current_size], text.text, max_length);
        current_size += (u32)max_length;

        // Add null termination for string.
        // By allocating one character for null termination this is safe
        data[current_size] = 0;
    }

    void StringBuffer::append(const StringBuffer& other_buffer) {
        if(other_buffer.current_size == 0) {
            return;
        }

        if(current_size + other_buffer.current_size >= buffer_size) {
            PASSERT_OVERFLOW();
            p_print("Buffer overflow. Please allocate more before appending");
            return;
        }

        memcpy(&data[current_size], other_buffer.data, other_buffer.current_size);
        current_size += other_buffer.current_size;
    }

    char* StringBuffer::append_use(const char* string) {
        return append_use_f("%s", string);
    }

    char* StringBuffer::append_use_f(const char* format, ...) {
        u32 cached_offset = this->current_size;

        // TODO: need a safer version
        if(current_size >= buffer_size) {
            PASSERT_OVERFLOW();
            p_print("Buffer full! Allocate more - append_use_f");
            return nullptr;
        }

        va_list args;
        va_start(args, format);
        int written_chars = vsnprintf_s(&data[current_size], buffer_size - current_size, _TRUNCATE, format, args);
        current_size += written_chars > 0 ? written_chars : 0;
        va_end(args);

        if(written_chars < 0) {
            p_print("New string is too big for current buffer. Allocate more size.\n");
        }

        data[current_size] = 0;
        current_size++;

        return this->data + cached_offset;
    }

    char* StringBuffer::append_use(const StringView& text) {
        u32 cached_offset = this->current_size;

        append(text);
        current_size++;

        return this->data + cached_offset;
    }

    char* StringBuffer::append_use_substring(cstring string, u32 start_index, u32 end_index) {
        u32 size = end_index - start_index;
        if(current_size + size >= buffer_size) {
            PASSERT_OVERFLOW();
            p_print("Buffer full!");
            return nullptr;
        }

        u32 cached_offset = this->current_size;

        memcpy(&data[current_size], string, size);
        current_size += size;

        data[current_size] = 0;
        current_size++;

        return this->data + cached_offset;
    }

    void StringBuffer::close_current_string() {
        data[current_size] = 0;
        current_size++;
    }

    u32 StringBuffer::get_index(cstring text) const {
        u64 text_distance = text - data;
        return text_distance < buffer_size ? u32(text_distance) : u32_max;
    }

    cstring StringBuffer::get_text(u32 index) const {
        return index < buffer_size ? cstring(data + index) : nullptr;
    }

    char* StringBuffer::reserve(size_t size) {
        if(current_size + size >= buffer_size) {
            return nullptr;
        }

        u32 offset = current_size;
        current_size += (u32) size;

        return data + offset;
    }

    void StringBuffer::clear() {
        current_size = 0;
        data[0] = 0;
    }

    // String Array ////
    void StringArray::init(Allocator* allocator_, size_t size) {
        allocator = allocator_;

        // Allocate also memory for the hash map
        size_t mem_size = size +  sizeof(FlatHashMap<u64, u32>) + sizeof(FlatHashMapIterator);
        char* allocated_memory = (char*)allocator->allocate(mem_size, 1);
        string_to_index = (FlatHashMap<u64, u32>*)allocated_memory;
        string_to_index->init(allocator, 8);
        string_to_index->set_default_value(u32_max);

        strings_iterator = (FlatHashMapIterator*)(allocated_memory + sizeof(FlatHashMap<u64, u32>));

        data = allocated_memory + sizeof(FlatHashMap<u64, u32>) + sizeof(FlatHashMapIterator);

        buffer_size = size;
        current_size = 0;
    }

    void StringArray::shutdown() {
        // string_to_index contains ALL the memory including data
        puffin_free(string_to_index, allocator);
        buffer_size = current_size = 0;
    }

    void StringArray::clear() {
        current_size = 0;
        string_to_index->clear();
    }

    FlatHashMapIterator* StringArray::begin_string_iteration() {
        *strings_iterator = string_to_index->iterator_begin();
        return strings_iterator;
    }

    size_t StringArray::get_string_count() const {
        return string_to_index->size;
    }

    cstring StringArray::get_next_string(FlatHashMapIterator* it) const {
        u32 index = string_to_index->get(*it);
        string_to_index->iterator_advance(*it);
        cstring string = get_string(index);
        return string;
    }

    bool StringArray::has_next_string(puffin::FlatHashMapIterator* it) const {
        return it->is_valid();
    }

    cstring StringArray::get_string(u32 index) const {
        u32 data_index = index;
        if(data_index < current_size) {
            return data + data_index;
        }
        return nullptr;
    }

    cstring StringArray::intern(cstring string) {
        static size_t seed = 0xf2ea4ffad;
        const size_t length = strlen(string);
        const size_t hashed_string = puffin::hash_bytes((void*)string, length, seed);

        u32 string_index = string_to_index->get(hashed_string);
        if(string_index != u32_max) {
            return data + string_index;
        }

        string_index = current_size;
        // Increase current buffer w/ interned string
        current_size += (u32)length + 1;
        strcpy(data + string_index, string);

        string_to_index->insert(hashed_string, string_index);

        return data + string_index;
    }

}
