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
        pipeline_creation.vertex_input.add_vertex_attribute( {0, 0, 0, VertexComponentFormat::Float3}); // position
        pipeline_creation.vertex_input.add_vertex_stream({0, 12, VertexInputRate::PerVertex});

        pipeline_creation.vertex_input.add_vertex_attribute({1, 1, 0, VertexComponentFormat::Float4}); // tangent
        pipeline_creation.vertex_input.add_vertex_stream({1, 16, VertexInputRate::PerVertex});

        pipeline_creation.vertex_input.add_vertex_attribute({2, 2, 0, VertexComponentFormat::Float3}); // normal
        pipeline_creation.vertex_input.add_vertex_stream({2, 12, VertexInputRate::PerVertex});

        pipeline_creation.vertex_input.add_vertex_attribute({3, 3, 0, VertexComponentFormat::Float2}); // texcoord
        pipeline_creation.vertex_input.add_vertex_stream({3, 8, VertexInputRate::PerVertex});

        // Render pass
        pipeline_creation.render_pass = gpu.get_swapchain_output();

        // Depth
        pipeline_creation.depth_stencil.set_depth(true, VK_COMPARE_OP_LESS_OR_EQUAL);

        // Shader state
        const char* vs_code = R"FOO(
            #version 450
            layout(std140, binding = 0) uniform LocalConstants {
                mat4 m;
                mat4 vp;
                mat4 mInverse;
                vec4 eye;
                vec4 light;
            };

            layout(location = 0) in vec3 position;
            layout(location = 1) in vec4 tangent;
            layout(location = 2) in vec3 normal;
            layout(location = 3) in vec2 texCoord0;

            layout(location = 0) out vec2 vTexCoord0;
            layout(location = 1) out vec3 vNormal;
            layout(location = 2) out vec4 vTangent;
            layout(location = 3) out vec4 vPosition;

            void main() {
                gl_Position = vp * m * vec4(position, 1);
                vPosition = m * vec4(position, 1.0);
                vTexCoord0 = texCoord0;
                vNormal = mat3(mInverse) * normal;
                vTangent = tangent;
            }
)FOO";

        const char* fs_code = R"FOO(
            #version 450
            layout(std140, binding = 0) uniform LocalConstants {
                mat4 m;
                mat4 vp;
                mat4 mInverse;
                vec4 eye;
                vec4 light;
            };

            layout(std140, binding = 4) uniform MaterialConstant {
                vec4 base_color_factor;
            };

            layout(binding = 1) uniform sampler2D diffuseTexture;
            layout(binding = 2) uniform sampler2D occlusionRoughnessMetalnessTexture;
            layout(binding = 3) uniform sampler2D normalTexture;

            layout(location = 0) in vec2 vTexcoord0;
            layout(location = 1) in vec3 vNormal;
            layout(location = 2) in vec4 vTangent;
            layout(location = 3) in vec4 vPosition;

            layout(location = 0) out vec4 frag_color;

            #define PI 3.1415926538

            vec3 decode_srgb(vec3 c) {
                vec3 result;
                if(c.r < 0.04045) {
                    result.r = c.r / 12.92;
                } else {
                    result.r = pow((c.r + 0.55) / 1.055, 2.4);
                }

                if(c.g < 0.04045) {
                    result.g = c.g / 12.92;
                } else {
                    result.g = pow((c.g + 0.55) / 1.055, 2.4);
                }

                if(c.b < 0.04045) {
                    result.b = c.b / 12.92;
                } else {
                    result.b = pow((c.b + 0.55) / 1.055, 2.4);
                }

                return clamp(result, 0.0, 1.0);
            }

            vec3 encode_srgb(vec3 c) {
                vec3 result;
                if(c.r <= 0.0031308) {
                    result.r = c.r * 12.92;
                } else {
                    result.r = 1.055 * pow(c.r, 1.0 / 2.4) - 0.055;
                }

                if(c.g <= 0.0031308) {
                    result.g = c.g * 12.92;
                } else {
                    result.g = 1.055 * pow(c.g, 1.0 / 2.4) - 0.055;
                }

                if(c.b <= 0.0031308) {
                    result.b = c.b * 12.92;
                } else {
                    result.b = 1.055 * pow(c.b, 1.0 / 2.4) - 0.055;
                }

                return clamp(result, 0.0, 1.0);
            }

            float heaviside(float v) {
                if(v > 0.0) {
                    return 1.0;
                }

                return 0.0;
            }

            void main() {
                vec3 bump_normal = normalize(texture(normalTexture, vTexcoord0).rgb * 2.0 - 1.0);
                vec3 tangent = normalize(vTangent.xyz);
                vec3 bitangent = cross(normalize(vNormal), tangent) * vTangent.w;

                mat3 TBN = transpose(mat3(
                    tangent,
                    bitangent,
                    normalize(vNormal)
                ));

                vec3 V = normalize(TBN * (eye.xyz - vPosition.xyz));
                vec3 L = normalize(TBN * (light.xyz - vPosition.xyz));
                vec3 N = bump_normal;
                vec3 H = normalize(L + V);

                vec4 rmo = texture(occlusionRoughnessMetalnessTexture, vTexcoord0);

                // Green channel contains roughness values
                float roughness = rmo.g;
                float alpha = pow(roughness, 2.0);

                // Blue channel contains metalness
                float metalness = rmo.b;

                // Red channel contains occlusion value
                vec4 base_color = texture(diffuseTexture, vTexcoord0) * base_color_factor;
                base_color.rgb = decode_srgb(base_color.rgb);

                // https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#specular-brdf
                float NdotH = dot(N, H);
                float alpha_squared = alpha * alpha;
                float d_denom = (NdotH * NdotH) * (alpha_squared - 1.0) + 1.0f;
                float distribution = (alpha_squared * heaviside(NdotH)) / (PI * d_denom * d_denom);

                float NdotL = dot(N, L);
                float NdotV = dot(N, V);
                float HdotL = dot(H, L);
                float NdotV = dot(H, V);

                float visibility = (heaviside(HdotL) / abs(NdotL) + sqrt(alpha_squared + (1.0 - alpha_squared) * (NdotL * NdotL)))) * (heaviside(HdotV) / (abs(NdotV) + sqrt(alpha_squared + (1.0 - alpha_squared) * (NdotV * NdotV))));

                float specular_brdf = visibility * distribution;

                vec3 diffuse_brdf = (1 / PI) * base_color.rgb;

                vec3 conductor_fresnel = specular_brdf * (base_color.rgb + (1.0 - base_color.rgb) * pow(1.0 - abs(HdotV), 5));

                float f0 = 0.04;
                float fr = f0 + (1 - f0) * pow(1 - abs(HdotV), 5);
                vec3 fresnel_mix = mix(diffuse_brdf, vec3(specular_brdf), fr);

                vec3 material_color = mix(fresnel_mix, conductor_fresnel, metalness);

                frag_color = vec4(encode_srgb(material_color), base_color.a);
            }
)FOO";

        pipeline_creation.shaders.set_name("Cube").add_stage(vs_code, (uint32_t)strlen(vs_code), VK_SHADER_STAGE_VERTEX_BIT)
                                                    .add_stage(fs_code, (uint32_t)strlen(fs_code), VK_SHADER_STAGE_FRAGMENT_BIT);

        // 





    }























}