//
// Created by darby on 5/10/2023.
//

#pragma once

#include "memory.hpp"
#include  "assert.hpp"

namespace puffin {

    template<typename T>
    struct Array {
        Array();

        ~Array();

        void init(Allocator* allocator, u32 initial_capacity, u32 initial_size = 0);

        void shutdown();

        void push(const T& element);

        T& push_use();                 // Grow the size and return T to be filled

        void pop();

        void delete_swap(u32 index);

        T& operator[](u32 index);       // can change the value returned from the array
        const T& operator[](u32 index) const; // useful when we want to consider the array "read-only"

        void clear();

        void set_size(u32 new_size);

        void set_capacity(u32 new_capacity); // WHATS THE DIFF?
        void grow(u32 new_capacity);

        T& back();

        const T& back() const;

        T& front();

        const T& front() const;

        u32 size_in_bytes() const;

        u32 capacity_in_bytes() const;

        T* data;
        u32 size;       // Occupied size
        u32 capacity;   // Allocated capacity
        Allocator* allocator;
    };

    // View over a contiguous memory block
    template<typename T>
    struct ArrayView {
        ArrayView(T* data, u32 size);

        void set(T* data, u32 size);

        T& operator[](u32 index);

        const T& operator[](u32 index) const;

        T* data;
        u32 size;
    };

    // Array
    // Array

    template<typename T>
    inline Array<T>::Array() {}

    template<typename T>
    inline Array<T>::~Array() {}

    template<typename T>
    inline void Array<T>::init(Allocator* allocator_, u32 initial_capacity, u32 initial_size) {
        data = nullptr;
        size = initial_size;
        capacity = 0;
        allocator = allocator_;

        if (initial_capacity > 0) {
            grow(initial_capacity);
        }
    }

    template<typename T>
    inline void Array<T>::shutdown() {
        if (capacity > 0) {
            allocator->deallocate(data);
        }

        data = nullptr;
        size = 0;
        capacity = 0;
    }

    template<typename T>
    inline void Array<T>::grow(u32 new_capacity) {
        if (new_capacity < capacity * 2) {
            new_capacity = capacity * 2;
        } else if (new_capacity < 4) {
            new_capacity = 4;
        }

        T* new_data = (T*) allocator->allocate(new_capacity * sizeof(T), alignof(T));
        if (capacity) {
            memory_copy(new_data, data, capacity * sizeof(T));
            allocator->deallocate(data);
        }

        data = new_data;
        capacity = new_capacity;
    }

    template<typename T>
    inline u32 Array<T>::capacity_in_bytes() const {
        return capacity * sizeof(T);
    }

    template<typename T>
    inline u32 Array<T>::size_in_bytes() const {
        return size * sizeof(T);
    }

    template<typename T>
    inline const T& Array<T>::front() const {
        PASSERT(size);
        return data[0];
    }

    template<typename T>
    inline T& Array<T>::front() {
        PASSERT(size);
        return data[0];
    }

    template<typename T>
    inline const T& Array<T>::back() const {
        PASSERT(size);
        return data[size - 1];
    }

    template<typename T>
    inline T& Array<T>::back() {
        PASSERT(size);
        return data[size - 1];
    }

    template<typename T>
    inline void Array<T>::set_capacity(u32 new_capacity) {
        if (new_capacity > capacity) {
            grow(new_capacity);
        }
    }

    template<typename T>
    inline void Array<T>::set_size(u32 new_size) {
        if (new_size > capacity) {
            grow(new_size);
        }
        size = new_size;
    }

    template<typename T>
    inline void Array<T>::clear() {
        size = 0;
    }

    template<typename T>
    inline const T& Array<T>::operator[](u32 index) const {
        PASSERT(index < size);
        return data[index];
    }

    template<typename T>
    inline T& Array<T>::operator[](u32 index) {
        PASSERT(index < size);
        return data[index];
    }

    template<typename T>
    inline void Array<T>::delete_swap(u32 index) {
        PASSERT(size > 0);
        size--;
        data[index] = data[size];
    }

    template<typename T>
    inline void Array<T>::pop() {
        PASSERT(size > 0);
        size--;
    }

    template<typename T>
    inline T& Array<T>::push_use() {
        if (size >= capacity) {
            grow(capacity + 1);
        }
        size++;

        return back();
    }

    template<typename T>
    inline void Array<T>::push(const T& element) {
        if (size >= capacity) {
            grow(capacity + 1);
        }

        data[size++] = element;
    }

    // ArrayView //////////////////////////////////////////////

    template<typename T>
    inline ArrayView<T>::ArrayView(T* data_, u32 size_)
        : data(data_), size(size_) {
    }

    template<typename T>
    inline const T& ArrayView<T>::operator[](u32 index) const {
        PASSERT(index  < size);
        return data[index];
    }

    template<typename T>
    inline T& ArrayView<T>::operator[](u32 index) {
        PASSERT(index < size);
        return data[index];
    }

    template<typename T>
    inline void ArrayView<T>::set(T* data_, u32 size_) {
        data = data_;
        size = size_;
    }
}