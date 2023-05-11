//
// Created by darby on 5/10/2023.
//

#pragma once

#include "memory.hpp"
#include  "assert.hpp"

namespace puffin {

    template <typename T>
    struct Array {
        Array();
        ~Array();

        void                    init(Allocator*  allocator, u32 initial_capacity, u32 initial_size = 0);
        void                    shutdown();

        void                    push(const T& element);
        T&                      push_use();                 // Grow the size and return T to be filled

        void                    pop();
        void                    delete_swap(u32 index);

        T&                      operator[](u32 index);       // can change the value returned from the array
        const T&                operator[](u32 index) const; // useful when we want to consider the array "read-only"

        void                    clear();
        void                    set_size(u32 new_size);
        void                    set_capacity(u32 new_capacity); // WHATS THE DIFF?
        void                    grow(u32 new_capacity);

        T&                      back();
        const T&                back() const;

        T&                      front();
        const T&                front() const;

        u32                     size_in_bytes() const;
        u32                     capacity_in_bytes() const;

        T*                      data;
        u32                     size;       // Occupied size
        u32                     capacity;   // Allocated capacity
        Allocator*              allocator;
    };

    template<typename T>
    void Array<T>::delete_swap(u32 index) {
        PASSERT(size > 0);
    }

    template<typename T>
    void Array<T>::pop() {
        PASSERT(size > 0);
        size--;
    }

    template<typename T>
    T& Array<T>::push_use() {
        if(size >= capacity) {
            grow(capacity + 1);
        }
        size++;

        return back();
    }

    template<typename T>
    void Array<T>::push(const T& element) {
        if(size >= capacity) {
            grow(capacity + 1);
        }

        data[size++] = element;
    }



    template<typename T>
    void Array<T>::shutdown() {
        if(capacity > 0) {
            allocator->deallocate(data);
        }

        data = nullptr;
        size = 0;
        capacity = 0;
    }




    // View over a contiguous memory block
    template <typename T>
    struct ArrayView {
        ArrayView(T* data, u32 size);

        void                set(T* data, u32 size);

        T&                  operator[](u32 index);
        const T&            operator[](u32 index) const;

        T*                  data;
        u32                 size;
    };
}