//
// Created by darby on 3/24/2023.
//

#include "memory.hpp"

#include "tlsf.h"
#include "imgui.h"

#include <cstdlib>
#include <cassert>
#include <iostream>

namespace puffin {

    static size_t s_size = puffin_mega(32) + tlsf_size() + 8;

    // Memory Service /////////////////////////////////////////////
    static MemoryService s_memory_service;
    MemoryService* MemoryService::instance() {
        return &s_memory_service;
    }

    // Memory Walkers for TLSF
    static void exit_walker(void* ptr, size_t size, int used, void* usr) {
        MemoryStatistics* stats = (MemoryStatistics*)usr;
        stats->add(used ? size : 0);

        if(used) {
            std::cout << "Found active allocation: " << ptr << ", size: " << size << std::endl;
        }
    }

    static void imgui_walker(void* ptr, size_t size, int used, void* usr) {
        u32 memory_size = (u32) size;
        cstring memory_unit = "b";
        if(memory_size > 1024 * 1024) {
            memory_size /= 1024 * 1024;
            memory_unit = "mB";
        } else if(memory_size > 1024)  {
            memory_size /= 1024;
            memory_unit = "kB";
        }

        ImGui::Text("\t%p %s size: %4llu %s \n", ptr, used ? "used" : "free", memory_size, memory_unit);

        MemoryStatistics* stats = (MemoryStatistics*) usr;
        stats->add(size);
    }

    // MEMORY FUNCTIONS
    void memory_copy(void* destination, void* source, size_t size) {
        memcpy(destination, source, size);
    }

    // Only works when parameters are powers of 2
    size_t memory_align(size_t size, size_t alignment) {
        const size_t alignment_mask = alignment - 1;
        return (size + alignment_mask) & ~alignment_mask;
    }

    // MEMORY SERVICE
    void MemoryService::init(void* configuration) {
        std::cout << "Memory Service Init" << std::endl;
        MemoryServiceConfiguration* config = (MemoryServiceConfiguration*) configuration;
        system_allocator.init(config ? config->maximum_dynamic_size : s_size);
    }

    void MemoryService::shutdown() {
        system_allocator.shutdown();
        std::cout << "Memory Service Shutdown" << std::endl;
    }

    void MemoryService::imgui_draw() {
        if(ImGui::Begin("Memory Service")) {
            system_allocator.debug_ui();
        }
        ImGui::End();
    }

    void MemoryService::test() {
        // No tests currently
    }

    // HEAP ALLOCATOR

    HeapAllocator::~HeapAllocator() {}

    void HeapAllocator::init(size_t size) {
        // Allocate
        memory = malloc(size);
        max_size = size;
        allocated_size = 0;

        tlsf_handle = tlsf_create_with_pool(memory, size);

        std::cout << "HeapAllocator of size [" << size << "] created" << std::endl;
    }

    void HeapAllocator::shutdown() {
        // Check for memory at application exit
        MemoryStatistics stats {0, max_size};
        pool_t pool = tlsf_get_pool(tlsf_handle);

        tlsf_walk_pool(pool, exit_walker, (void*) &stats);

        tlsf_destroy(tlsf_handle);
        free(memory);
    }

    void HeapAllocator::debug_ui() {
        ImGui::Separator();
        ImGui::Text("Heap Allocator");
        ImGui::Separator();
        MemoryStatistics stats{0, max_size};
        pool_t pool = tlsf_get_pool(tlsf_handle);
        tlsf_walk_pool(pool, imgui_walker, (void*) &stats);

        ImGui::Separator();
        ImGui::Text("\tAllocation count %d", stats.allocation_count);
        ImGui::Text("\tAllocated %llu K, free %llu Mb, count %d Mb",
                    stats.allocated_bytes / (1024 * 1024),
                    (max_size - stats.allocated_bytes) / (1024 * 1024),
                    max_size / (1024 * 1024));
    }

    void* HeapAllocator::allocate(size_t size, size_t alignment) {
        void* allocated_memory = alignment == 1 ? tlsf_malloc(tlsf_handle, size)
                : tlsf_memalign(tlsf_handle, alignment, size);

        size_t actual_size = tlsf_block_size(allocated_memory);
        allocated_size += actual_size;
        return allocated_memory;
    }

    void* HeapAllocator::allocate(size_t size, size_t alignment, cstring file, i32 line) {
        return allocate(size, alignment);
    }

    void HeapAllocator::deallocate(void *pointer) {
        size_t actual_size = tlsf_block_size(pointer);
        allocated_size -= actual_size;
        tlsf_free(tlsf_handle, pointer);
    }

    // LINEAR ALLOCATOR

    LinearAllocator::~LinearAllocator() { }

    void LinearAllocator::init(size_t size) {
        memory = (u8*)malloc(size);
        total_size = size;
        allocated_size = 0;
    }

    void LinearAllocator::shutdown() {
        clear();
        free(memory);
    }

    void* LinearAllocator::allocate(size_t size, size_t alignment) {
        assert(size > 0);
        const size_t new_start = memory_align(size, alignment);
        assert(new_start < total_size);
        const size_t new_allocated_size = new_start + size;
        assert(new_allocated_size > total_size);
        allocated_size = new_allocated_size;
        return memory + new_start;
    }

    void* LinearAllocator::allocate(size_t size, size_t alignment, cstring file, i32 line) {
        return allocate(size, alignment);
    }

    void LinearAllocator::deallocate(void *pointer) {
        std::cout << "Linear allocator attempting to deallocate on a pointer-basis" << std::endl;
    }

    void LinearAllocator::clear() {
        allocated_size = 0;
    }

    // STACK ALLOCATOR

    void StackAllocator::init( size_t size ) {
        memory = (u8*) malloc(size);
        allocated_size = 0;
        total_size = 0;
    }

    void StackAllocator::shutdown() const {
        free(memory);
    }

    void* StackAllocator::allocate(size_t size, size_t alignment) {
        assert(size > 0);
        const size_t new_start = memory_align(allocated_size, alignment);
        assert(new_start < total_size);
        const size_t new_allocated_size = new_start + size;
        assert(new_allocated_size < total_size);
        allocated_size = new_allocated_size;
        return memory + new_start;
    }

    void* StackAllocator::allocate(size_t size, size_t alignment, cstring file, i32 line) {
        return allocate(size, alignment);
    }

    void StackAllocator::deallocate(void *pointer) {
        assert(pointer >= memory);
        assert(pointer < memory + total_size);
        assert(pointer < memory + allocated_size);

        const size_t size_at_pointer = (u8*) pointer - memory;
        allocated_size = size_at_pointer;
    }

    size_t StackAllocator::get_marker() {
        return allocated_size;
    }

    void StackAllocator::free_marker(size_t marker) {
        const size_t difference = marker - allocated_size;
        if(difference > 0) {
            allocated_size = marker;
        }
    }

    void StackAllocator::clear() {
        allocated_size = 0;
    }
}