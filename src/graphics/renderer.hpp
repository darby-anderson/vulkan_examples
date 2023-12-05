//
// Created by darby on 12/3/2023.
//

#pragma once

#include "gpu_device.hpp"
#include "vulkan_resources.hpp"
#include "resource_manager.hpp"

namespace puffin {

struct Renderer;

struct BufferResource : public puffin::Resource {

    BufferHandle            handle;
    u32                     pool_index;
    BufferDescription       desc;

    static constexpr cstring k_type = "puffin_buffer_type";
    static u64               k_type_hash;

};

struct TextureResource : public puffin::Resource {

    TextureHandle           handle;
    u32                     pool_index;
    TextureDescription      desc;

    static constexpr cstring k_type = "puffin_texture_type";
    static u64               k_type_hash;

};

struct SamplerResource : public puffin::Resource {

    SamplerHandle           handle;
    u32                     pool_index;
    SamplerDescription      desc;

    static constexpr cstring k_type = "puffin_sampler_type";
    static u64               k_type_hash;

};

// Resource Cache ///////

struct ResourceCache {
    void                    init(Allocator* allocate);
    void                    shutdown(Renderer* renderer);

    FlatHashMap<u64, TextureResource*>  textures;
    FlatHashMap<u64, BufferResource*>   buffers;
    FlatHashMap<u64, SamplerResource*>  samplers;
};

// Renderer ////////////////

struct RendererCreation {
    puffin::GpuDevice*      gpu;
    Allocator*              allocator;};

//
// Class in charge of high level resources
//
struct Renderer : public Service {

    PUFFIN_DECLARE_SERVICE(Renderer);

    void                    init(const RendererCreation& creation);
    void                    shutdown();

    void                    set_loaders(puffin::ResourceManager* manager);

    void                    begin_frame();
    void                    end_frame();

    void                    resize_swapchain(u32 width, u32 height);

    f32                     aspect_ratio() const;

    // Creation/destruction
    BufferResource*         create_buffer(const BufferCreation& creation);
    BufferResource*         create_buffer(VkBufferUsageFlags type, ResourceUsageType::Enum usage, u32 size, void* data, cstring name);

    TextureResource*        create_texture(const TextureCreation& creation);
    TextureResource*        create_texture(cstring name, cstring filename);

    SamplerResource*        create_sampler(const SamplerCreation& creation);

    void                    destroy_buffer(BufferResource* buffer);
    void                    destroy_texture(TextureResource* buffer);
    void                    destroy_sampler(SamplerResource* buffer);

    // Update resource
    void*                   map_buffer(BufferResource* buffer, u32 offset = 0, u32 size = 0);
    void                    unmap_buffer(BufferResource* buffer);
    CommandBuffer*          get_command_buffer(QueueType::Enum type, bool begin) { return gpu->get_command_buffer(type, begin); }

    ResourcePoolTyped<TextureResource> textures;
    ResourcePoolTyped<BufferResource> buffers;
    ResourcePoolTyped<SamplerResource> samplers;

    ResourceCache           resource_cache;

    puffin::GpuDevice*      gpu;

    u16                     width;
    u16                     height;

    static constexpr cstring k_name = "puffin_rendering_service";
};

}
