//
// Created by darby on 5/10/2023.
//

#include "array.hpp"

namespace puffin {

    template<typename T>
    inline Array<T>::Array() {

    }

    template<typename T>
    Array<T>::~Array() {

    }

    template<typename T>
    inline void Array<T>::init(Allocator* allocator_, u32 initial_capacity, u32 initial_size) {
        data = nullptr;
        size = initial_size;
        capacity = 0;
        allocator = allocator_;

        if(initial_capacity > 0) {
            grow(initial_capacity);
        }
    }



    template<typename T>
    inline void Array<T>::grow(u32 new_capacity) {
        if(new_capacity < capacity * 2) {
            new_capacity = capacity * 2;
        } else if (new_capacity < 4) {
            new_capacity = 4;
        }

        T* new_data = (T*) allocator->allocate(new_capacity * sizeof(T), alignof(T));
        if(capacity) {
            memory_copy(new_data, data, capacity * sizeof(T));
            allocator->deallocate(data);
        }

        data = new_data;
        capacity = new_capacity;
    }

}
