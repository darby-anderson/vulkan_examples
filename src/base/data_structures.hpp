//
// Created by darby on 6/1/2023.
//

#pragma once

#include "memory.hpp"
#include "assert.hpp"

namespace puffin {

    /*
     * EXAMPLE RUNTHROUGH
     *
     * [--p_size*type--][--p_size*u32--]
        ^	   	  ^
        |		  |
        memory		  free_indices

        start (p_size = 3):
        free_indices_head = 0
        used_indices = 0
        free_indices = [0, 1, 2]
                ^
                |
                free_indices_head
                (everything after this is an available index)

        obtain_resource():
        free_index = free_indices[f_i_h] = 0
        free_indices_head = 1
        used_indices = 1
        return 0

        obtain_resource():
        free_index = freeindices[f_i_h] = 1
        free_indices_head = 2
        used_indices = 2
        return 1

        release_resource(0):
        free_indices_head  = 1
        free_indices[f_i_h(1)] = 0
        used_indices = 1

        free_indices = [0, 0, 2]
                   ^
                   |
                   f_i_h
     */

    struct ResourcePool {
        void            init(Allocator* allocator, u32 pool_size, u32 resource_size);
        void            shutdown();

        u32             obtain_resource(); // returns an index to the resource
        void            release_resource(u32 index);
        void            release_all_resources();

        void*           access_resource(u32 index);
        const void*     access_resource(u32 index) const;

        u8*             memory = nullptr;
        u32*            free_indices = nullptr;
        Allocator*      allocator = nullptr;

        u32             free_indices_head   = 0;
        u32             pool_size           = 16;
        u32             resource_size       = 4;
        u32             used_indices        = 0;
    };

    template <typename T>
    struct ResourcePoolTyped : public ResourcePool {
        void            init(Allocator* allocator, u32 pool_size);
        void            shutdown();

        T*              obtain();
        void            release(T* resource);

        T*              get(u32 index);
        const T*        get(u32 index) const;
    };

    template<typename T>
    inline void ResourcePoolTyped<T>::init(puffin::Allocator* allocator, u32 pool_size) {
        ResourcePool::init(allocator, pool_size, sizeof(T));
    }

    template<typename T>
    inline void ResourcePoolTyped<T>::shutdown() {
        if(free_indices_head != 0) {
            p_print("Resource pool as unreleased resources! \n");

            for(u32 i = 0; i < free_indices_head; i++) {
                p_print("\t Resource %u, %s, \n", free_indices[i], get(free_indices[i]) -> name);
            }

            ResourcePool::shutdown();
        }
    }

    template<typename T>
    inline T* ResourcePoolTyped<T>::obtain() {
        u32 resource_index = ResourcePool::obtain_resource();
        if(resource_index == u32_max) { return nullptr; }

        T* resource = get(resource_index);
        resource->pool_index = resource_index;
        return resource;
    }

    template<typename T>
    inline void ResourcePoolTyped<T>::release(T* resource) {
        ResourcePool::release_resource(resource->pool_index);
    }

    template<typename T>
    inline T* ResourcePoolTyped<T>::get(u32 index) {
        return (T*) ResourcePool::access_resource(index);
    }

    template<typename T>
    inline const T* ResourcePoolTyped<T>::get(u32 index) const {
        return (const T*) ResourcePool::access_resource(index);
    }

}