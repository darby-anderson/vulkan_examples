//
// Created by darby on 2/26/2023.
//
#pragma once

#include "gpu_device.hpp"

namespace puffin {

// Command Buffers are submitted to queues for execution.
// They can be baked for reuse
struct CommandBuffer {

    void                    init(QueueType::Enum type, u32 buffer_size, u32 submit_size, bool baked);
    void                    terminate();

    // Commands interface

    DescriptorSetHandle     create_descriptor_set(const DescriptorSetCreation& creation);

    void                    bind_pass(RenderPassHandle render_pass_handle);
    void                    bind_pipeline(PipelineHandle pipeline_handle);
    void                    bind_vertex_buffer(BufferHandle buffer_handle, u32 binding, u32 offset);
    void                    bind_index_buffer(BufferHandle buffer_handle, u32 offset, VkIndexType index_type);
    void                    bind_descriptor_set(DescriptorSetHandle* descriptor_set_handles, u32 num_lists, u32* offsets, u32 num_offsets);
    void                    bind_local_descriptor_set(DescriptorSetHandle* descriptor_set_handles, u32 num_lists, u32* offsets, u32 num_offsets);

    void                    set_viewport(const Viewport* viewport);
    void                    set_scissors(const Rect2DInt* rect); // sets what part of viewport is being rendered on

    void                    clear(f32 red, f32 green, f32 blue, f32 alpha);
    void                    clear_depth_stencil(f32 depth, u8 stencil);
    // depth is used for calculating occlusion
    // stencils help to create shadows and reflections

    void                    draw(TopologyType::Enum topology, u32 first_vertex, u32 vertex_count, u32 first_instance, u32 instance_count);
    void                    draw_indexed(TopologyType::Enum topology, u32 index_count, u32 instance_count, u32 first_index,
                                         i32 vertex_offset, u32 first_instance);
    void                    draw_indirect(BufferHandle buffer_handle, u32 offset, u32 stride);
    void                    draw_indexed_indirect(BufferHandle buffer_handle, u32 offset, u32 stride);

    void                    dispatch(u32 group_x, u32 group_y, u32 group_z);
    void                    dispatch_indirect(BufferHandle buffer_handle, u32 offset);

    void                    barrier(const ExecutionBarrier& barrier);

    void                    fill_buffer(BufferHandle buffer_handle, u32 offset, u32 size, u32 data);

    void                    push_marker(const char* name);
    void                    pop_marker();

    void                    reset();

    VkCommandBuffer         vk_command_buffer;

    VkDescriptorPool        vk_descriptor_pool;
    ResourcePool            descriptor_sets;

    GpuDevice*              gpu_device;

    VkDescriptorSet         vk_descriptor_sets[16];

    RenderPass*             current_render_pass;
    Pipeline*               current_pipeline;
    VkClearValue            clears[2];              // 0 = color, 1 = depth_stencil
    bool                    is_recording;

    u32                     handle;

    u32                     current_command;
    ResourceHandle          resource_handle;
    QueueType::Enum         type                = QueueType::Graphics;
    u32                     buffer_size         = 0;

    bool                    baked               = false; // If baked, reset() will only affect the read of the commands

};

};

