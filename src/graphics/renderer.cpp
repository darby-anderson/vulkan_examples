//
// Created by darby on 12/3/2023.
//

#include "renderer.hpp"
#include "command_buffer.hpp"

#include "memory.hpp"
#include "file_system.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "config.h"

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

// Material Creation
MaterialCreation& MaterialCreation::reset() {
    program = nullptr;
    name = nullptr;
    render_index = ~0u;
    return *this;
}

MaterialCreation& MaterialCreation::set_program(Program* program_) {
    program = program_;
    return *this;
}

MaterialCreation& MaterialCreation::set_render_index(u32 render_index_) {
    render_index = render_index_;
    return *this;
}

MaterialCreation& MaterialCreation::set_name(cstring name_) {
    name = name_;
    return *this;
}


static TextureHandle create_texture_from_file(GpuDevice& gpu, cstring filename, cstring name, bool create_mipmaps) {
    if(filename) {
        int comp, width, height;
        uint8_t* image_data = stbi_load(filename, &width, &height, &comp, 4);
        if(!image_data) {
            p_print("Error loading texture %s", filename);
            return k_invalid_texture;
        }

        u32 mip_levels = 1;
        if(create_mipmaps) {
            u32 w = width;
            u32 h = height;

            while(w > 1 && h > 1) {
                w /= 2;
                h /= 2;

                mip_levels++;
            }
        }

        TextureCreation creation;
        creation.set_data(image_data)
            .set_format_type(VK_FORMAT_R8G8B8A8_UNORM, TextureType::Texture2D)
            .set_flags(mip_levels, 0).set_size((u16)width, (u16)height, 1)
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
    programs.init(creation.allocator, 128);
    materials.init(creation.allocator, 128);

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

void Renderer::set_loaders(ResourceManager* manager) {
    manager->set_loader(TextureResource::k_type, &s_texture_loader);
    manager->set_loader(BufferResource::k_type, &s_buffer_loader);
    manager->set_loader(SamplerResource::k_type, &s_sampler_loader);
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

TextureResource* Renderer::create_texture(cstring name, cstring filename, bool create_mipmaps) {
    TextureResource* texture = textures.obtain();

    if(!texture) {
        return nullptr;
    }

    TextureHandle handle = create_texture_from_file(*gpu, filename, name, create_mipmaps);
    texture->handle = handle;
    gpu->query_texture(handle, texture->desc);
    texture->references = 1;
    texture->name = name;

    resource_cache.textures.insert(hash_calculate(name), texture);
    return texture;
}

SamplerResource* Renderer::create_sampler(const SamplerCreation& creation) {
    SamplerResource* sampler = samplers.obtain();

    if(sampler) {
        SamplerHandle handle = gpu->create_sampler(creation);
        sampler->handle = handle;
        sampler->name = creation.name;
        gpu->query_sampler(handle, sampler->desc);

        if(creation.name != nullptr) {
            resource_cache.samplers.insert(hash_calculate(creation.name), sampler);
        }

        sampler->references = 1;

        return sampler;
    }

    return nullptr;
}

Program* Renderer::create_program(const ProgramCreation& creation) {
    Program* program = programs.obtain();

    if(program) {
        const u32 num_passes = 1;
        program->passes.init(gpu->allocator, num_passes, num_passes);

        program->name = creation.pipeline_creation.name;

        StringBuffer pipeline_cache_path;
        pipeline_cache_path.init(gpu->allocator, 1024);

        for(u32 i = 0; i < num_passes; i++) {
            ProgramPass& pass = program->passes[i];

            if(creation.pipeline_creation.name != nullptr) {
                char* cache_path = pipeline_cache_path.append_use_f("%s%s.cache", PUFFIN_SHADER_FOLDER, creation.pipeline_creation.name);
                pass.pipeline = gpu->create_pipeline(creation.pipeline_creation, cache_path);
            } else {
                pass.pipeline = gpu->create_pipeline(creation.pipeline_creation);
            }

            pass.descriptor_set_layout = gpu->access_descriptor_set_layout(pass.pipeline, 0);
        }

        pipeline_cache_path.shutdown();

        if(creation.pipeline_creation.name != nullptr) {
            resource_cache.programs.insert(hash_calculate(creation.pipeline_creation.name), program);
        }

        program->references = 1;

        return program;
    }

    return nullptr;
}

Material* Renderer::create_material(const MaterialCreation& creation) {
    Material* material materials.obtain();

    if(material) {
        material->program = creation.program;
        material->name = creation.name;
        material->render_index = creation.render_index;

        if(creation.name != nullptr) {
            resource_cache.m
        }
    }

}

void Renderer::destroy_buffer(BufferResource* buffer) {
    if(!buffer) {
        return;
    }

    buffer->remove_reference();
    if(buffer->references) {
        return;
    }

    resource_cache.buffers.remove(hash_calculate(buffer->desc.name));
    gpu->destroy_buffer(buffer->handle);
    buffers.release(buffer);
}

void Renderer::destroy_texture(TextureResource* texture) {
    if(!texture) {
        return;
    }

    texture->remove_reference();
    if(texture->references) {
        return;
    }

    resource_cache.textures.remove(hash_calculate(texture->desc.name));
    gpu->destroy_texture(texture->handle);
    textures.release(texture);
}

void Renderer::destroy_sampler(SamplerResource* sampler) {
    if(!sampler) {
        return;
    }

    sampler->remove_reference();
    if(sampler->references) {
        return;
    }

    resource_cache.samplers.remove(hash_calculate(sampler->desc.name));
    gpu->destroy_sampler(sampler->handle);
    samplers.release(sampler);
}

void* Renderer::map_buffer(BufferResource* buffer, u32 offset, u32 size) {
    MapBufferParameters cb_map = {buffer->handle, offset, size };
    return gpu->map_buffer(cb_map);
}

void Renderer::unmap_buffer(BufferResource* buffer) {
    if(buffer->desc.parent_handle.index == k_invalid_index) {
        MapBufferParameters cb_map = {buffer->handle, 0, 0};
        gpu->unmap_buffer(cb_map);
    }
}

// Resource Loaders ///////
// Texture Loader //////
Resource* TextureLoader::get(cstring name) {
    const u64 hashed_name = hash_calculate(name);
    return renderer->resource_cache.textures.get(hashed_name);
}

Resource* TextureLoader::get(u64 hashed_name) {
    return renderer->resource_cache.textures.get(hashed_name);
}

Resource* TextureLoader::unload(cstring name) {
    const u64 hashed_name = hash_calculate(name);
    TextureResource* texture = renderer->resource_cache.textures.get(hashed_name);
    if(texture) {
        renderer->destroy_texture(texture);
    }
    return nullptr;
}

Resource* TextureLoader::create_from_file(cstring name, cstring filename, ResourceManager* resource_manager) {
    return renderer->create_texture(name, filename);
}

// Buffer Loader /////
Resource* BufferLoader::get(cstring name) {
    const u64 hashed_name = hash_calculate(name);
    return renderer->resource_cache.buffers.get(hashed_name);
}

Resource* BufferLoader::get(u64 hashed_name) {
    return renderer->resource_cache.buffers.get(hashed_name);
}

Resource* BufferLoader::unload(cstring name) {
    const u64 hashed_name = hash_calculate(name);
    BufferResource* buffer = renderer->resource_cache.buffers.get(hashed_name);
    if(buffer) {
        renderer->destroy_buffer(buffer);
    }
    return nullptr;
}

// Sampler Loader ////////////
Resource* SamplerLoader::get(cstring name) {
    const u64 hashed_name = hash_calculate(name);
    return renderer->resource_cache.samplers.get(hashed_name);
}

Resource* SamplerLoader::get(u64 hashed_name) {
    return renderer->resource_cache.samplers.get(hashed_name);
}

Resource* SamplerLoader::unload(cstring name) {
    const u64 hashed_name = hash_calculate(name);
    SamplerResource* sampler = renderer->resource_cache.samplers.get(hashed_name);
    if(sampler) {
        renderer->destroy_sampler(sampler);
    }
    return nullptr;
}

// Resource Cache

void ResourceCache::init(Allocator* allocator) {
    // Init resources caching
    textures.init(allocator, 16);
    buffers.init(allocator, 16);
    samplers.init(allocator, 16);
    materials.init(allocator, 16);
    programs.init(allocator, 16);
}

void ResourceCache::shutdown(puffin::Renderer* renderer) {
    FlatHashMapIterator it = textures.iterator_begin();
    while(it.is_valid()) {
        TextureResource* texture = textures.get(it);
        renderer->destroy_texture(texture);

        textures.iterator_advance(it);
    }

    it = buffers.iterator_begin();
    while(it.is_valid()) {
        BufferResource* buffer = buffers.get(it);
        renderer->destroy_buffer(buffer);

        buffers.iterator_advance(it);
    }

    it = samplers.iterator_begin();
    while(it.is_valid()) {
        SamplerResource* sampler = samplers.get(it);
        renderer->destroy_sampler(sampler);

        samplers.iterator_advance(it);
    }

    it = materials.iterator_begin();
    while(it.is_valid()) {
        Material* material = materials.get(it);
        renderer->destroy_material(material);

        materials.iterator_advance(it);
    }

    it = programs.iterator_begin();
    while(it.is_valid()) {
        Program* program = programs.get(it);
        renderer->destroy_program(program);

        programs.iterator_advance(it);
    }

    textures.shutdown();
    buffers.shutdown();
    samplers.shutdown();
    materials.shutdown();
    programs.shutdown();
}


}
