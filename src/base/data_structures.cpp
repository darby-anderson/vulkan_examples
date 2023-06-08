//
// Created by darby on 6/1/2023.
//

#include "data_structures.hpp"

#include <string.h>

namespace puffin {

    static const u32            k_invalid_index = 0xffffffff;

    // Resource Pool /////////////////
    void ResourcePool::init(Allocator* allocator_, u32 pool_size_, u32 resource_size_) {
        allocator = allocator_;
        pool_size = pool_size_;
        resource_size = resource_size_;

        // Group allocate
        size_t allocation_size = pool_size * (resource_size + sizeof(u32)); // used status is stored on the tail?
        memory = (u8*) allocator->allocate(allocation_size, 1);
        memset(memory, 0, allocation_size);

        // Allocate and free indices
        free_indices = (u32*) (memory + pool_size * resource_size);
        free_indices_head = 0;

        for(u32 i = 0; i < pool_size; i++) {
            free_indices[i] = i;
        }

        used_indices = 0;
    }

    void ResourcePool::shutdown() {
        if(free_indices_head != 0) {
            p_print("Resource pool has unreleased resources.\n");

            for(u32 i = 0; i < free_indices_head; i++) {
                p_print("\tResource %u\n", free_indices[i]);
            }
        }

        PASSERT(used_indices == 0);
        allocator->deallocate(memory);
    }

    void ResourcePool::release_all_resources() {
        free_indices_head = 0;
        used_indices = 0;

        for(u32 i = 0; i < pool_size; i++) {
            free_indices[i] = i;
        }
    }

    u32 ResourcePool::obtain_resource() {
        if(free_indices_head >= pool_size) {
            PASSERT(false);
            return k_invalid_index;
        }

        const u32 free_index = free_indices[free_indices_head];
        free_indices_head++;
        used_indices++;
        return free_index;
    }

    void ResourcePool::release_resource(u32 index) {
        free_indices_head--;
        free_indices[free_indices_head] = index;
        used_indices--;
    }

    void* ResourcePool::access_resource(u32 index) {
        if(index == k_invalid_index) { return nullptr; }

        return &memory[index * resource_size];
    }

    const void* ResourcePool::access_resource(u32 index) const {
        if(index == k_invalid_index) { return nullptr; }

        return &memory[index * resource_size];
    }

}