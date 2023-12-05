//
// Created by darby on 12/3/2023.
//

#include "renderer.hpp"
#include "command_buffer.hpp"

#include "memory.hpp"
#include "file_system.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace puffin {

// Resource Loaders ///

struct TextureLoader : public puffin::ResourceLoader {
    Resource*           get(cstring name) override;
    Resource*           get(u64 hashd_name) override;

    Resource*           unload(cstring name) override;

    Resource*           create_from_file(cstring name, cstring filename, ResourceManager* resource_manager) override;

    Renderer*           renderer;
};

struct BufferLoader : public puffin::ResourceLoader {
    Resource*           get(cstring name) override;
    Resource*           get(u64 hashed_name) override;

    Resource*           unload(cstring name) override;

    Renderer*           renderer;
};

struct SamplerLoader : public puffin::ResourceLoader {
    Resource*           get(cstring name) override;
    Resource*           get(u64 hashed_name) override;

    Resource*           unload(cstring name) override;

    Renderer*           renderer;
};

static TextureHandle create_texture_from_file(GpuDevice& gpu, cstring filename, cstring name) {
    if(filename) {
        int comp, width, height;
        uint8_t* image_data = stbi_load(filename, &width, &height, &comp, 4);
        if(!image_data) {
            p_print("Error loading texture %s", filename);
            return k_invalid_texture;
        }

        TextureCreation creation;
        creation.set_data(image_data)
            .set_format_type(VK_FORMAT_R8G8B8A8_UNORM, TextureType::Texture2D)
            .set_flags(1, 0).set_size((u16)width, (u16)height, 1)
            .set_name(name);

        TextureHandle new_texture = gpu.create_texture(creation);

        free(image_data);

        return new_texture;
    }

    return k_invalid_texture;
}

// Renderer ///////////////

u64 TextureResource::k_type_hash = 0;
u64 BufferResource::k_type_hash = 0;
u64 SamplerResource::k_type_hash = 0;

static TextureLoader s_texture_loader;
static BufferLoader s_buffer_loader;
static SamplerLoader s_sampler_loader;

static Renderer s_renderer;

Renderer* Renderer::instance() {
    return &s_renderer;
}

void Renderer::init(const RendererCreation& creation) {
    p_print("Renderer init\n");

    gpu = creation.gpu;

    width = gpu->swapchain_width;
    height = gpu->swapchain_height;

    textures.init(creation.allocator, 512);
    buffers.init(creation.allocator, 512);
    samplers.init(creation.allocator, 128);

    resource_cache.init(creation.allocator);

    // Init resource hashes
    TextureResource::k_type_hash = hash_calculate(TextureResource::k_type);
    BufferResource::k_type_hash = hash_calculate(BufferResource::k_type);
    SamplerResource::k_type_hash = hash_calculate(SamplerResource::k_type);

    s_texture_loader.renderer = this;
    s_buffer_loader.renderer = this;
    s_sampler_loader.renderer = this;
}

void Renderer::shutdown() {
    resource_cache.shutdown(this);

    textures.shutdown();
    buffers.shutdown();
    samplers.shutdown();

    p_print("Renderer shutdown\n");

    gpu->shutdown();
}

void Renderer::begin_frame() {
    gpu->new_frame();
}

void Renderer::end_frame() {
    gpu->present();
}

void Renderer::resize_swapchain(u32 width_, u32 height_) {
    gpu->resize((u16) width_, (u16) height_);

    width = gpu->swapchain_width;
    height = gpu->swapchain_height;
}

f32 Renderer::aspect_ratio() const {
    return gpu->swapchain_width * 1.f / gpu->swapchain_height;
}

BufferResource* Renderer::create_buffer(const BufferCreation& creation) {
    BufferResource* buffer = buffers.obtain();
    if(buffer) {
        BufferHandle handle = gpu->create_buffer(creation);
        buffer->handle = handle;
        buffer->name = creation.name;
        gpu->query_buffer(handle, buffer->desc);

        if(creation.name != nullptr) {
            resource_cache.buffers.insert(hash_calculate(creation.name), buffer);
        }

        buffer->references = 1;

        return buffer;
    }
    return nullptr;
}

BufferResource* Renderer::create_buffer(VkBufferUsageFlags type, ResourceUsageType::Enum usage, u32 size, void* data,
                                        cstring name) {
    BufferCreation creation {type, usage, size, data, name};
    return create_buffer(creation);
}

TextureResource* Renderer::create_texture(const TextureCreation& creation) {
    TextureResource* texture = textures.obtain();

    if(!texture) {
        return nullptr;
    }

    TextureHandle handle = gpu->create_texture(creation);
    texture->handle = handle;
    texture->name = creation.name;
    gpu->query_texture(handle, texture->desc);

    if(creation.name != nullptr) {
        resource_cache.textures.insert(hash_calculate(creation.name), texture);
    }

    texture->references = 1;
    return texture;
}

TextureResource* Renderer::create_texture(cstring name, cstring filename) {
    TextureResource* texture = textures.obtain();

    if(!texture) {
        return nullptr;
    }

    TextureHandle handle = create_texture_from_file(*gpu, filename, name);
    texture->handle = handle;
    gpu->query_texture(handle, texture->desc);
    texture->references = 1;
    texture->name = name;

    resource_cache.textures.insert(hash_calculate(name), texture);
    return texture;
}



}
