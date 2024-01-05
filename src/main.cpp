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

#include "tracy/Tracy.hpp"

#include "file_system.hpp"
#include "gltf.hpp"
#include "numerics.hpp"
#include "resource_manager.hpp"
#include "time.hpp"

#include <stdlib.h>


// Rotating cube test
puffin::BufferHandle                cube_vb;
puffin::BufferHandle                cube_ib;
puffin::PipelineHandle              cube_pipeline;
puffin::BufferHandle                cube_cb;
puffin::DescriptorSetHandle         cube_rl;
puffin::DescriptorSetLayoutHandle   cube_dsl;

f32 rx, ry;

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

    // input service
    InputConfiguration i_conf { };
    InputService input_handler;
    input_handler.init(&i_conf);

    // window
    WindowConfiguration w_conf { 1200, 800, "Puffin Window", allocator, &input_handler};
    Window window;
    window.init(&w_conf);

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

        // Descriptor set layout
        DescriptorSetLayoutCreation cube_rll_creation{};
        cube_rll_creation.add_binding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 1, "LocalConstants"});
        cube_rll_creation.add_binding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1, "diffuseTexture"});
        cube_rll_creation.add_binding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, 1, "occlusionRoughnessMetalnessTexture"});
        cube_rll_creation.add_binding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, 1, "normalTexture"});
        cube_rll_creation.add_binding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4, 1, "MaterialConstants"});

        // Setting it into pipeline
        cube_dsl = gpu.create_descriptor_set_layout(cube_rll_creation);
        pipeline_creation.add_descriptor_set_layout(cube_dsl);

        // Constant buffer
        BufferCreation buffer_creation;
        buffer_creation.reset().set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceUsageType::Dynamic, sizeof(UniformData)).set_name("cube_cb");
        cube_cb = gpu.create_buffer(buffer_creation);

        cube_pipeline = gpu.create_pipeline(pipeline_creation);

        for(u32 mesh_index = 0; mesh_index < scene.meshes_count; mesh_index++) {
            MeshDraw mesh_draw{};
            glTF::Mesh& mesh = scene.meshes[mesh_index];

            for(u32 primitive_index = 0; primitive_index < mesh.primitives_count; primitive_index++) {
                glTF::MeshPrimitive& mesh_primitive = mesh.primitives[primitive_index];

                i32 position_accessor_index = gltf_get_attribute_accessor_index(mesh_primitive.attributes, mesh_primitive.attribute_count, "POSITION");
                i32 tangent_accessor_index = gltf_get_attribute_accessor_index(mesh_primitive.attributes, mesh_primitive.attribute_count, "TANGENT");
                i32 normal_accessor_index = gltf_get_attribute_accessor_index(mesh_primitive.attributes, mesh_primitive.attribute_count, "NORMAL");
                i32 texcoord_accessor_index = gltf_get_attribute_accessor_index(mesh_primitive.attributes, mesh_primitive.attribute_count, "TEXCOORD_0");

                if(position_accessor_index != -1) {
                    glTF::Accessor& position_accessor = scene.accessors[position_accessor_index];
                    glTF::BufferView& position_buffer_view = scene.buffer_views[position_accessor.buffer_view];
                    BufferResource& position_buffer_gpu = buffers[position_accessor.buffer_view];

                    mesh_draw.position_buffer = position_buffer_gpu.handle;
                    mesh_draw.position_offset = position_accessor.byte_offset == glTF::INVALID_INT_VALUE ? 0 : position_accessor.byte_offset;
                }

                if(tangent_accessor_index != -1) {
                    glTF::Accessor& tangent_accessor = scene.accessors[tangent_accessor_index];
                    glTF::BufferView& tangent_buffer_view = scene.buffer_views[tangent_accessor.buffer_view];
                    BufferResource& tangent_buffer_gpu = buffers[tangent_accessor.buffer_view];

                    mesh_draw.tangent_buffer = tangent_buffer_gpu.handle;
                    mesh_draw.tangent_offset = tangent_accessor.byte_offset == glTF::INVALID_INT_VALUE ? 0 : tangent_accessor.byte_offset;
                }

                if(normal_accessor_index != -1) {
                    glTF::Accessor& normal_accessor = scene.accessors[normal_accessor_index];
                    glTF::BufferView& normal_buffer_view = scene.buffer_views[normal_accessor.buffer_view];
                    BufferResource& normal_buffer_gpu = buffers[normal_accessor.buffer_view];

                    mesh_draw.normal_buffer = normal_buffer_gpu.handle;
                    mesh_draw.normal_offset = normal_accessor.byte_offset == glTF::INVALID_INT_VALUE ? 0 : normal_accessor.byte_offset;
                }

                if(texcoord_accessor_index != -1) {
                    glTF::Accessor& texcoord_accessor = scene.accessors[texcoord_accessor_index];
                    glTF::BufferView& texcoord_buffer_view = scene.buffer_views[texcoord_accessor.buffer_view];
                    BufferResource& texcoord_buffer_gpu = buffers[texcoord_accessor.buffer_view];

                    mesh_draw.texcoord_buffer = texcoord_buffer_gpu.handle;
                    mesh_draw.texcoord_offset = texcoord_accessor.byte_offset == glTF::INVALID_INT_VALUE ? 0 : texcoord_accessor.byte_offset;
                }

                glTF::Accessor& indices_accessor = scene.accessors[mesh_primitive.indices];
                glTF::BufferView& indices_buffer_view = scene.buffer_views[indices_accessor.buffer_view];
                BufferResource& indices_buffer_gpu = buffers[indices_accessor.buffer_view];
                mesh_draw.index_buffer = indices_buffer_gpu.handle;
                mesh_draw.index_offset = indices_accessor.byte_offset == glTF::INVALID_INT_VALUE ? 0 : indices_accessor.byte_offset;

                glTF::Material& material = scene.materials[mesh_primitive.material];

                // Descriptor set
                DescriptorSetCreation ds_creation{};
                ds_creation.set_layout(cube_dsl).buffer(cube_cb, 0);

                if(material.pbr_metallic_roughness != nullptr) {
                    if(material.pbr_metallic_roughness->base_color_factor_count != 0) {
                        PASSERT(material.pbr_metallic_roughness->base_color_factor_count == 4);

                        mesh_draw.material_data.base_color_factor = {
                                material.pbr_metallic_roughness->base_color_factor[0],
                                material.pbr_metallic_roughness->base_color_factor[1],
                                material.pbr_metallic_roughness->base_color_factor[2],
                                material.pbr_metallic_roughness->base_color_factor[3],
                        };
                    } else {
                        mesh_draw.material_data.base_color_factor = { 1.0f, 1.0f, 1.0f, 1.0f };
                    }

                    if(material.pbr_metallic_roughness->base_color_texture != nullptr) {
                        glTF::Texture& diffuse_texture = scene.textures[material.pbr_metallic_roughness->base_color_texture->index];
                        TextureResource& diffuse_texture_gpu = images[diffuse_texture.source];
                        SamplerResource& diffuse_sampler_gpu = samplers[diffuse_texture.sampler];

                        ds_creation.texture_sampler(diffuse_texture_gpu.handle, diffuse_sampler_gpu.handle, 1);
                    } else {
                        continue;
                    }

                    if(material.pbr_metallic_roughness->metallic_roughness_texture != nullptr) {
                        glTF::Texture& roughness_texture = scene.textures[material.pbr_metallic_roughness->metallic_roughness_texture->index];
                        TextureResource& roughness_texture_gpu = images[roughness_texture.source];
                        SamplerResource& roughness_sampler_gpu = samplers[roughness_texture.sampler];

                        ds_creation.texture_sampler(roughness_texture_gpu.handle, roughness_sampler_gpu.handle, 2);
                    } else if(material.occlusion_texture != nullptr) {
                        glTF::Texture& occlusion_texture = scene.textures[material.occlusion_texture->index];
                        TextureResource& occlusion_texture_gpu = images[occlusion_texture.source];
                        SamplerResource& occlusion_sampler_gpu = samplers[occlusion_texture.sampler];

                        ds_creation.texture_sampler(occlusion_texture_gpu.handle, occlusion_sampler_gpu.handle, 2);
                    } else {
                        continue;
                    }

                } else {
                    continue;
                }

                if(material.normal_texture != nullptr) {
                    glTF::Texture& normal_texture = scene.textures[material.normal_texture->index];
                    TextureResource& normal_texture_gpu = images[normal_texture.source];
                    SamplerResource& normal_sampler_gpu = samplers[normal_texture.sampler];

                    ds_creation.texture_sampler(normal_texture_gpu.handle, normal_sampler_gpu.handle, 3);
                } else {
                    continue;
                }

                buffer_creation.reset().set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceUsageType::Dynamic, sizeof(MaterialData)).set_name("material");
                mesh_draw.material_buffer = gpu.create_buffer(buffer_creation);
                ds_creation.buffer(mesh_draw.material_buffer, 4);

                mesh_draw.count = indices_accessor.count;

                mesh_draw.descriptor_set = gpu.create_descriptor_set(ds_creation);

                mesh_draws.push(mesh_draw);
            }

        }

        rx = 0.0f;
        ry = 0.0f;

    }

    i64 begin_frame_tick = time_now();

    vec3s eye = vec3s { 0.0f, 2.5f, 2.0f };
    vec3s look = vec3s { 0.0f, 0.0f, -1.0f };
    vec3s right = vec3s { 1.0f, 0.0f, 0.0f };

    f32 yaw = 0.0f;
    f32 pitch = 0.0f;

    float model_scale = 0.008f;

    while(!window.should_exit()) {

        ZoneScoped;

        // New frame
        if(!window.minimized) {
            gpu.new_frame();
        }

        window.request_os_messages();

        if(window.resized) {
            gpu.resize(window.width, window.height);
            window.resized = false;
        }

        imgui->new_frame();

        const i64 current_tick = time_now();
        f32 delta_time = (f32) time_delta_seconds(begin_frame_tick, current_tick);
        begin_frame_tick = current_tick;

        input_handler.start_new_frame();

        if(ImGui::Begin("Puffin ImGui")) {
            ImGui::InputFloat("Model scale", &model_scale, 0.001f);
        }
        ImGui::End();

        if(ImGui::Begin("GPU")) {
            gpu_profiler.imgui_draw();
        }
        ImGui::End();

        {
            // Update rotating cube data
            MapBufferParameters cb_map = { cube_cb, 0, 0 };
            float* cb_data = (float*)gpu.map_buffer(cb_map);

            if(cb_data) {
                if(input_handler.is_mouse_button_down(MouseButton::MOUSE_BUTTON_LEFT)) {
                    pitch += (input_handler.mouse_position.y - input_handler.previous_mouse_position.y) * 0.1f;
                    yaw += (input_handler.mouse_position.x - input_handler.previous_mouse_position.x) * 0.3f;

                    pitch = clamp(pitch, -60.0f, 60.0f);

                    if(yaw > 360.0f) {
                        yaw -= 360.0f;
                    }

                    mat3s rxm = glms_mat4_pick3( glms_rotate_make(glm_rad(-pitch), vec3s {1.0f, 0.0f, 0.0f }));
                    mat3s rym = glms_mat4_pick3(glms_rotate_make(glm_rad(yaw), vec3s {0.0f, 1.0f, 0.0f}));

                    look = glms_mat3_mulv(rxm, vec3s{0.0f, 0.0f, -1.0f});
                    look = glms_mat3_mulv(rym, look);

                    right = glms_cross(look, vec3s {0.0f, 1.0f, 0.0f});
                }

                if(input_handler.is_key_down(Key::KEY_W)) {
                    eye = glms_vec3_add(eye, glms_vec3_scale(look, 5.0f * delta_time));
                } else if(input_handler.is_key_down(Key::KEY_S)){
                    eye = glms_vec3_sub(eye, glms_vec3_scale(look, 5.0f * delta_time));
                }

                if(input_handler.is_key_down(Key::KEY_D)) {
                    eye = glms_vec3_add(eye, glms_vec3_scale(right, 5.0f * delta_time));
                } else if(input_handler.is_key_down(Key::KEY_A)){
                    eye = glms_vec3_sub(eye, glms_vec3_scale(look, 5.0f * delta_time));
                }

                mat4s view = glms_lookat(eye, glms_vec3_add(eye, look), vec3s{0.0f, 1.0f, 0.0f});
                mat4s projection = glms_perspective(glm_rad(60.0f), gpu.swapchain_width * 1.0f / gpu.swapchain_height, 0.01f, 1000.0f);



            }
        }
    }
























}