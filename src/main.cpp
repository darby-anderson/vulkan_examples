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
#include "graphics/gpu_profiler.hpp"

#include "cglm/struct/mat3.h"
#include "cglm/struct/mat4.h"
#include "cglm/struct/cam.h"
#include "cglm/struct/affine.h"

#include "imgui.h"

#include "file_system.hpp"
#include "gltf.hpp"
#include "numerics.hpp"
#include "resource_manager.hpp"
#include "time.hpp"

#include <stdlib.h>

struct MaterialData {
    vec4s                   base_color_factor;
};

struct MeshDraw {
    puffin::BufferHandle    index_buffer;
    puffin::BufferHandle    position_buffer;
    puffin::BufferHandle    tangent_buffer;
    puffin::BufferHandle    normal_buffer;
    puffin::BufferHandle    texcoord_buffer;

    puffin::BufferHandle    material_buffer;
    MaterialData            material_data;

    u32                     index_offset;
    u32                     position_offset;
    u32                     tangent_offset;
    u32                     normal_offset;
    u32                     texcoord_offset;

    u32                     count;

    puffin::DescriptorSetHandle     descriptor_set;
};

struct UniformData {
    mat4s m;
    mat4s vp;
    mat4s inverseM;
    vec4s eye;
    vec4s light;
};

int main(int argc, char** argv) {

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

    GPUProfiler gpu_profiler;
    gpu_profiler.init(allocator, 100);

    Renderer renderer;
    renderer.init({&gpu, allocator});
    renderer.set_loaders(&rm);

    ImGuiService* imgui = ImGuiService::instance();
    ImGuiServiceConfiguration imgui_config {&gpu, window.platform_handle};
    imgui->init(&imgui_config);

    Directory cwd{};
    directory_current(&cwd);

    char gltf_base_path[512] {};
    memcpy(gltf_base_path, argv[1], strlen(argv[1]));
    file_directory_from_path(gltf_base_path);

    directory_change(gltf_base_path);

    char gltf_file[512] {};
    memcpy(gltf_file, argv[1], strlen(argv[1]));
    file_name_from_path(gltf_file);

    glTF::glTF scene = gltf_load_file(gltf_file);

    Array<TextureResource> images;
    images.init(allocator, scene.images_count);

    for(u32 image_index = 0; image_index < scene.images_count; image_index++) {
        glTF::Image& image = scene.images[image_index];
        TextureResource* tr = renderer.create_texture(image.uri.data, image.uri.data);
        PASSERT(tr != nullptr);

        images.push(*tr);
    }

    StringBuffer resource_name_buffer;
    resource_name_buffer.init(allocator, 4096);

    Array<SamplerResource> samplers;
    samplers.init(allocator, scene.samplers_count);

    for(u32 sampler_index = 0; sampler_index < scene.samplers_count; sampler_index++) {
        glTF::Sampler& sampler = scene.samplers[sampler_index];

        char* sampler_name = resource_name_buffer.append_use_f("sampler_%u", sampler_index);

        SamplerCreation creation;
        creation.min_filter = sampler.min_filter == glTF::Sampler::Filter::LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        creation.mag_filter = sampler.mag_filter == glTF::Sampler::Filter::LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        creation.name = sampler_name;

        SamplerResource* sr = renderer.create_sampler(creation);
        PASSERT(sr == nullptr);

        samplers.push(*sr);
    }

    Array<void*> buffers_data;
    buffers_data.init(allocator, scene.buffers_count);

    for(u32 buffer_index = 0; buffer_index < scene.buffers_count; buffer_index++) {
        glTF::Buffer& buffer = scene.buffers[buffer_index];

        FileReadResult buffer_data = file_read_binary(buffer.uri.data, allocator);
        buffers_data.push(buffer_data.data);
    }

    Array<BufferResource> buffers;
    buffers.init(allocator, scene.buffer_views_count);

    for(u32 buffer_index = 0; buffer_index < scene.buffer_views_count; buffer_index++) {
        glTF::BufferView& buffer = scene.buffer_views[buffer_index];

        i32 offset = buffer.byte_offset;
        if(offset == glTF::INVALID_INT_VALUE) {
            offset = 0;
        }

        u8* data = (u8*) buffers_data[buffer.buffer] + offset;

        VkBufferUsageFlags flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        char* buffer_name = buffer.name.data;
        if(buffer_name == nullptr) {
            buffer_name = resource_name_buffer.append_use_f("buffer_%u", buffer_index);
        }

        BufferResource* br = renderer.create_buffer(flags, ResourceUsageType::Immutable, buffer.byte_length, data, buffer_name);
        PASSERT(br != nullptr);

        buffers.push(*br);
    }

    for(u32 buffer_index = 0; buffer_index < scene.buffers_count; buffer_index++) {
        void* buffer = buffers_data[buffer_index];
        allocator->deallocate(buffer);
    }
    buffers_data.shutdown();

    directory_change(cwd.path);

    Array<MeshDraw> mesh_draws;
    mesh_draws.init(allocator, scene.meshes_count);

    {
        // Create pipeline state
        PipelineCreation pipeline_creation;

        // Vertex input
        pipeline_creation.vertex_input.add_vertex_attribute( {0, 0, 0, VertexComponentFormat::Float3});
        pipeline_creation.vertex_input.add_vertex_stream({0, 12, VertexInputRate::PerVertex});




    }























}