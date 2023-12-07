//
// Created by darby on 5/10/2023.
//

/*
 * Main file using re-written engine with more modular structure.
 * Based on Sascha Willem and Mastering Graphics Programming with Vulkan
 */

#include "application/window.hpp"
#include "application/input.hpp"
#include "application/keys.hpp"

#include "graphics/gpu_device.hpp"
#include "graphics/command_buffer.hpp"
#include "graphics/renderer.hpp"
#include "graphics/puffin_imgui.hpp"



int main() {

    // Init services

    using namespace puffin;

    MemoryService::instance()->init(nullptr);

    Allocator* allocator = &MemoryService::instance()->system_allocator;

    StackAllocator scratch_allocator;
    scratch_allocator.init(puffin_mega(8));

    // window
    WindowConfiguration w_conf { 1200, 800, "Puffin Window", allocator};
    Window window;
    window.init(&w_conf);

    InputConfiguration i_conf { &window };
    InputService input_handler;
    input_handler.init(&i_conf);

    // graphics
    DeviceCreation dc;
    dc.set_window(window.width, window.height, window.platform_handle).set_allocator(allocator).set_linear_allocator(&scratch_allocator);

    GpuDevice gpu;
    gpu.init(dc);

    ResourceManager rm;
    rm.init(allocator, nullptr);

    // GPUProfiler gpu_profiler;
    // gpu_profiler.ini

}