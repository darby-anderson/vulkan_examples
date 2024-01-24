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
#include "cglm/struct/quat.h"
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

enum MaterialFeatures {
    MaterialFeatures_ColorTexture       = 1 << 0,
    MaterialFeatures_NormalTexture      = 1 << 1,
    MaterialFeatures_RoughnessTexture   = 1 << 2,
    MaterialFeatures_OcclusionTexture   = 1 << 3,
    MaterialFeatures_EmissiveTexture    = 1 << 4,

    MaterialFeatures_TangentVertexAttribute     = 1 << 5,
    MaterialFeatures_TexcoordVertexAttribute    = 1 << 6,
};

struct alignas(16) MaterialData {
    vec4s   base_color_factor;
    mat4s   model;
    mat4s   model_inv;

    vec3s   emissive_factor;
    f32     metallic_factor;

    f32     roughness_factor;
    f32     occulsion_factor;
    u32     flags;
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

    VkIndexType             index_type;

    puffin::DescriptorSetHandle     descriptor_set;
};

struct UniformData {
    mat4s m;
    mat4s vp;
    vec4s eye;
    vec4s light;
};

struct Transform {
    vec3s       scale;
    versors     rotation;
    vec3s       translation;

    void        reset();
    mat4s       calculate_matrix() const {
        const mat4s translation_matrix = glms_translate_make(translation);
        const mat4s scale_matrix = glms_scale_make(scale);
        const mat4s rotation_matrix = glms_quat_mat4(rotation);

        const mat4s local_matrix = glms_mat4_mul(glms_mat4_mul(translation_matrix, rotation_matrix), scale_matrix);
        return local_matrix;
    }
};

static u8* get_buffer_data(puffin::glTF::BufferView* buffer_views, u32 buffer_index, puffin::Array<void*>& buffers_data, u32* buffer_size = nullptr, char** buffer_name = nullptr) {
    using namespace puffin;

    glTF::BufferView& buffer = buffer_views[buffer_index];

    i32 offset = buffer.byte_offset;
    if(offset == glTF::INVALID_INT_VALUE) {
        offset = 0;
    }

    if(buffer_name != nullptr) {
        *buffer_name = buffer.name.data;
    }

    if(buffer_size != nullptr) {
        *buffer_size = buffer.byte_length;
    }

    u8* data = (u8*) buffers_data[buffer.buffer] + offset;

    return data;
}

int main(int argc, char** argv) {

    if(argc < 2) {
        printf("Usage: project [path to gltf model]");
        InjectDefault3DModel();
    }

    // Init services

    using namespace puffin;

    time_service_init();

    MemoryService::instance()->init(nullptr);

    Allocator* allocator = &MemoryService::instance()->system_allocator;

    StackAllocator scratch_allocator;
    scratch_allocator.init(puffin_mega(8));

    // window
    WindowConfiguration w_conf { 1200, 800, "Puffin Window", allocator };
    Window window;
    window.init(&w_conf);

    // input service
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

    TextureCreation texture_creation {};
    u32 zero_value =0;
    texture_creation.set_name("dummy_texture").set_size(1, 1, 1)
        .set_format_type(VK_FORMAT_R8G8B8A8_UNORM, TextureType::Texture2D).set_flags(1, 0).set_data(&zero_value);
    TextureHandle dummy_texture = gpu.create_texture(texture_creation);

    SamplerCreation sampler_creation {};
    sampler_creation.min_filter = VK_FILTER_LINEAR;
    sampler_creation.mag_filter = VK_FILTER_LINEAR;
    sampler_creation.address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_creation.address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    SamplerHandle dummy_sampler = gpu.create_sampler(sampler_creation);

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
        PASSERT(sr != nullptr);

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
        char* buffer_name = nullptr;
        u32 buffer_size = 0 ;
        u8* data = get_buffer_data(scene.buffer_views, buffer_index, buffers_data, &buffer_size, &buffer_name);

        VkBufferUsageFlags flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        if(buffer_name == nullptr) {
            buffer_name = resource_name_buffer.append_use_f("buffer_%u", buffer_index);
        } else {
            buffer_name = resource_name_buffer.append_use_f("%s_%u", buffer_name, buffer_index);

        }

        BufferResource* br = renderer.create_buffer(flags, ResourceUsageType::Immutable, buffer_size, data, buffer_name);
        PASSERT(br != nullptr);

        buffers.push(*br);
    }

    directory_change(cwd.path);

    Array<MeshDraw> mesh_draws;
    mesh_draws.init(allocator, scene.meshes_count);

    Array<BufferHandle> custom_mesh_buffers{};
    custom_mesh_buffers.init(allocator, 8);

    vec4s dummy_data[3] {};
    BufferCreation buffer_creation{};
    buffer_creation.set(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, ResourceUsageType::Immutable, sizeof(vec3s) * 3).set_data(dummy_data).set_name("dummy_attribute_buffer");

    BufferHandle dummy_attribute_buffer = gpu.create_buffer(buffer_creation);

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

            uint MaterialFeatures_ColorTexture              = 1 << 0;
            uint MaterialFeatures_NormalTexture             = 1 << 1;
            uint MaterialFeatures_RoughnessTexture          = 1 << 2;
            uint MaterialFeatures_OcculusionTexture         = 1 << 3;
            uint MaterialFeatures_EmissiveTexture           = 1 << 4;
            uint MaterialFeatures_TangentVertexAttribute    = 1 << 5;
            uint MaterialFeatures_TexcoordVertexAttribute   = 1 << 6;

            layout(std140, binding = 0) uniform LocalConstants {
                mat4 m;
                mat4 vp;
                vec4 eye;
                vec4 light;
            };

            layout(std140, binding = 1) uniform MaterialConstant {
                vec4 base_color_factor;
                mat4 model;
                mat4 model_inv;

                vec3 emissive_factor;
                float metallic_factor;

                float roughness_factor;
                float occulsion_factor;
                uint flags;
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
                gl_Position = vp * m * model * vec4(position, 1);
                vPosition = m * model * vec4(position, 1.0);
                vNormal = mat3(model_inv) * normal;

                if((flags & MaterialFeatures_TexcoordVertexAttribute) != 0) {
                    vTexcoord0 = texCoord0;
                }

                if((flags & MaterialFeatures_TangentVertexAttribute) != 0) {
                    vTangent = tangent;
                }
            }
)FOO";

        const char* fs_code = R"FOO(
            #version 450

            uint MaterialFeatures_ColorTexture              = 1 << 0;
            uint MaterialFeatures_NormalTexture             = 1 << 1;
            uint MaterialFeatures_RoughnessTexture          = 1 << 2;
            uint MaterialFeatures_OcculusionTexture         = 1 << 3;
            uint MaterialFeatures_EmissiveTexture           = 1 << 4;
            uint MaterialFeatures_TangentVertexAttribute    = 1 << 5;
            uint MaterialFeatures_TexcoordVertexAttribute   = 1 << 6;

            layout(std140, binding = 0) uniform LocalConstants {
                mat4 m;
                mat4 vp;
                mat4 mInverse;
                vec4 eye;
                vec4 light;
            };

            layout(std140, binding = 1) uniform MaterialConstant {
                vec4 base_color_factor;
                mat4 model;
                mat4 model_inv;

                vec3 emissive_factor;
                float metallic_factor;

                float roughness_factor;
                float occulsion_factor;
                uint flags;
            };

            layout(binding = 2) uniform sampler2D diffuseTexture;
            layout(binding = 3) uniform sampler2D roughnessMetalnessTexture;
            layout(binding = 4) uniform sampler2D occlusionTexture;
            layout(binding = 5) uniform sampler2D emissiveTexture;
            layout(binding = 6) uniform sampler2D normalTexture;

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

                mat3 TBN = mat3(1.0);

                if((flags & MaterialFeatures_TangentVertexAttribute) != 0) {
                    vec3 tangent = normalize(vTangent.xyz);
                    vec3 bitangent = cross( normalize(vNormal), tangent) * vTangent.w;

                    TBN = mat3(
                        tangent,
                        bitangent,
                        normalize(vNormal)
                    );
                } else {
                    // dFdx and dFdy return the partial derivative of an argument with respect to x or y
//                    Concentrating on the Fine variant. As each fragment process reaches the dFd* call
//                    the GPU will collect the values passed in and based on those values,
//                    typically through getting the difference between neighbouring values and dividing by the fragment size.

                    vec3 Q1 = dFdx(vPosition.xyz);
                    vec3 Q2 = dFdy(vPosition.xyz);
                    vec2 st1 = dFdx(vTexcoord0);
                    vec2 st2 = dFdy(vTexcoord0);

                    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
                    vec3 B = normalize(-Q1 * st2.s + Q2 * st1.s);

                    mat3 TBN = mat3(
                        T,
                        B,
                        normalize(vNormal)
                    );
                }

                vec3 V = normalize(eye.xyz - vPosition.xyz);
                vec3 L = normalize(light.xyz - vPosition.xyz);
                vec3 N = normalize(vNormal);

                if((flags & MaterialFeatures_NormalTexture) != 0) {
                    N = normalize(texture(normalTexture, vTexcoord0).rgb * 2.0 - 1.0);
                    N = normalize(TBN * N);
                }

                vec3 H = normalize(L + V);

                float roughness = roughness_factor;
                float metalness = metalness_factor;

                if((flags & MaterialFeatures_RoughnessTexture) != 0) {
                    // red channel for occlusion
                    // green channel for roughness
                    // blue channel for metalness

                    vec4 rm = texture(roughnessMetalnessTexture, vTexcoord0);

                    roughness *= rm.g;
                    metalness *= rm.b;
                }

                float ao = 1.0f;
                if((flags & MetalFeatures_OcclusionTexture) != 0) {
                    ao = texture(occlusionTexture, vTexcoord0).r;
                }

                float alpha = pow(roughness, 2.0f);

                vec4 base_color = base_color_factor;
                if((flags & MaterialFeatures_ColorTexture) != 0) {
                    vec4 albedo = texture(diffuseTexture, vTexcoord0);
                    base_color.rgb *= decode_srgb(albedo.rgb);
                    base_color.a *= albedo.a;
                }

                vec3 emissive = vec3(0);
                if((flags & MaterialFeatures_EmissiveTexture) != 0) {
                    vec4 e = texture(emissiveTexture, vTexcoord0);

                    emissive += decode_srgb(e.rgb) * emissive_factor;
                }

                // https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#specular-brdf
                float NdotH = dot(N, H);
                float alpha_squared = alpha * alpha;
                float d_denom = (NdotH * NdotH) * (alpha_squared - 1.0) + 1.0f;
                float distribution = (alpha_squared * heaviside(NdotH)) / (PI * d_denom * d_denom);

                float NdotL = clamp(dot(N, L), 0, 1);

                if(NdotL > 1e-5) {
                    float NdotV = dot(N, V);
                    float HdotL = dot(H, L);
                    float HdotV = dot(H, V);

                    float visibility = (heaviside(HdotL) / (abs(NdotL) + sqrt(alpha_squared + (1.0 - alpha_squared) * (NdotL * NdotL)))) * (heaviside(HdotV) / (abs(NdotV) + sqrt(alpha_squared + (1.0 - alpha_squared) * (NdotV * NdotV))));

                    float specular_brdf = visibility * distribution;

                    vec3 diffuse_brdf = (1 / PI) * base_color.rgb;

                    vec3 conductor_fresnel = specular_brdf * (base_color.rgb + (1.0 - base_color.rgb) * pow(1.0 - abs(HdotV), 5));

                    float f0 = 0.04;
                    float fr = f0 + (1 - f0) * pow(1 - abs(HdotV), 5);
                    vec3 fresnel_mix = mix(diffuse_brdf, vec3(specular_brdf), fr);

                    vec3 material_color = emissive + mix(fresnel_mix, conductor_fresnel, metalness);

                    frag_color = vec4(encode_srgb(material_color), base_color.a);
                } else {
                    frag_color = vec4(base_color.rgb * 0.1, base_color.a);
                }
            }
)FOO";

        pipeline_creation.shaders.set_name("Cube").add_stage(vs_code, (uint32_t)strlen(vs_code), VK_SHADER_STAGE_VERTEX_BIT)
                                                    .add_stage(fs_code, (uint32_t)strlen(fs_code), VK_SHADER_STAGE_FRAGMENT_BIT);

        // Descriptor set layout
        DescriptorSetLayoutCreation cube_rll_creation{};
        cube_rll_creation.add_binding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 1, "LocalConstants"});
        cube_rll_creation.add_binding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 1, "MaterialConstant"});

        cube_rll_creation.add_binding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, 1, "diffuseTexture"});
        cube_rll_creation.add_binding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, 1, "roughnessMetalnessTexture"});
        cube_rll_creation.add_binding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, 1, "roughnessMetalnessTexture"});
        cube_rll_creation.add_binding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, 1, "emissiveTexture"});
        cube_rll_creation.add_binding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6, 1, "occlusionTexture"});

        // Setting it into pipeline
        cube_dsl = gpu.create_descriptor_set_layout(cube_rll_creation);
        pipeline_creation.add_descriptor_set_layout(cube_dsl);

        // Constant buffer
        BufferCreation buffer_creation;
        buffer_creation.reset().set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceUsageType::Dynamic, sizeof(UniformData)).set_name("cube_cb");
        cube_cb = gpu.create_buffer(buffer_creation);

        cube_pipeline = gpu.create_pipeline(pipeline_creation);

        glTF::Scene& root_gltf_scene = scene.scenes[scene.scene];

        Array<i32> node_parents;
        node_parents.init(allocator, scene.nodes_count, scene.nodes_count);

        Array<u32> node_stack;
        node_stack.init(allocator, 8);

        Array<mat4s> node_matrix;
        node_matrix.init(allocator, scene.nodes_count, scene.nodes_count);

        for(u32 node_index = 0; node_index < root_gltf_scene.nodes_count; node_index++) {
            u32 root_node = root_gltf_scene.nodes[node_index];
            node_parents[root_node] = -1;
            node_stack.push(root_node);
        }

        while(node_stack.size) {
            u32 node_index = node_stack.back();
            node_stack.pop();
            glTF::Node& node = scene.nodes[node_index];

            mat4s local_matrix{};

            if(node.matrix_count) {
                memcpy(&local_matrix, node.matrix, sizeof(mat4s));
            } else {
                vec3s node_scale {1.0f, 1.0f, 1.0f};

                if(node.scale_count != 0) {
                    PASSERT(node.scale_count == 3);
                    node_scale = vec3s{node.scale[0], node.scale[1], node.scale[2]};
                }

                vec3s node_translation {0.f, 0.f, 0.f};
                if(node.translation_count != 0) {
                    PASSERT(node.translation_count == 3);
                    node_translation = vec3s{node.translation[0], node.translation[1], node.translation[2]};
                }

                versors node_rotation = glms_quat_identity();
                if(node.rotation_count != 0) {
                    PASSERT(node.rotation_count == 4);
                    node_rotation = glms_quat_init(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
                }

                Transform transform;
                transform.translation = node_translation;
                transform.scale = node_scale;
                transform.rotation = node_rotation;

                local_matrix = transform.calculate_matrix();
            }

            node_matrix[node_index] = local_matrix;

            for(u32 child_index = 0; child_index < node.children_count; child_index++) {
                u32 child_node_index = node.children[child_index];
                node_parents[child_node_index] = node_index;
                node_stack.push(child_node_index);
            }

            if(node.mesh == glTF::INVALID_INT_VALUE) {
                continue;
            }

            glTF::Mesh& mesh = scene.meshes[node.mesh];

            mat4s final_matrix = local_matrix;
            i32 node_parent = node_parents[node_index];
            while(node_parent != -1) {
                final_matrix = glms_mat4_mul(node_matrix[node_parent], final_matrix);
                node_parent = node_parents[node_parent];
            }

            for(u32 primitive_index = 0; primitive_index < mesh.primitives_count; primitive_index++) {
                MeshDraw mesh_draw{};

                mesh_draw.material_data.model = final_matrix;

                glTF::MeshPrimitive& mesh_primitive = mesh.primitives[primitive_index];

                glTF::Accessor& indices_accessor = scene.accessors[mesh_primitive.indices];
                PASSERT(indices_accessor.component_type == glTF::Accessor::UNSIGNED_INT || indices_accessor.component_type == glTF::Accessor::UNSIGNED_SHORT);
                mesh_draw.index_type = indices_accessor.component_type == glTF::Accessor::UNSIGNED_INT ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;

                glTF::BufferView& indices_buffer_view = scene.buffer_views[indices_accessor.buffer_view];
                BufferResource& indices_buffer_gpu = buffers[indices_accessor.buffer_view];
                mesh_draw.index_buffer = indices_buffer_gpu.handle;
                mesh_draw.index_offset = indices_accessor.byte_offset== glTF::INVALID_INT_VALUE ? 0 : indices_accessor.byte_offset;
                mesh_draw.count = indices_accessor.count;

                PASSERT((mesh_draw.count % 3) == 0);

                i32 position_accessor_index = gltf_get_attribute_accessor_index(mesh_primitive.attributes, mesh_primitive.attribute_count, "POSITION");
                i32 tangent_accessor_index = gltf_get_attribute_accessor_index(mesh_primitive.attributes, mesh_primitive.attribute_count, "TANGENT");
                i32 normal_accessor_index = gltf_get_attribute_accessor_index(mesh_primitive.attributes, mesh_primitive.attribute_count, "NORMAL");
                i32 texcoord_accessor_index = gltf_get_attribute_accessor_index(mesh_primitive.attributes, mesh_primitive.attribute_count, "TEXCOORD_0");

                vec3s* position_data = nullptr;
                u32* index_data_32 = (u32*) get_buffer_data(scene.buffer_views, indices_accessor.buffer_view, buffers_data);
                u16* index_data_16 = (u16*) index_data_32;
                u32 vertex_count = 0;

                if(position_accessor_index != -1) {
                    glTF::Accessor& position_accessor = scene.accessors[position_accessor_index];
                    glTF::BufferView& position_buffer_view = scene.buffer_views[position_accessor.buffer_view];
                    BufferResource& position_buffer_gpu = buffers[position_accessor.buffer_view];

                    vertex_count = position_accessor.count;

                    mesh_draw.position_buffer = position_buffer_gpu.handle;
                    mesh_draw.position_offset = position_accessor.byte_offset == glTF::INVALID_INT_VALUE ? 0 : position_accessor.byte_offset;

                    position_data = (vec3s*)get_buffer_data(scene.buffer_views, position_accessor.buffer_view, buffers_data);
                } else {
                    PASSERTM(false, "No position data found!");
                    continue;
                }

                if(normal_accessor_index != -1) {
                    glTF::Accessor& normal_accessor = scene.accessors[normal_accessor_index];
                    glTF::BufferView& normal_buffer_view = scene.buffer_views[normal_accessor.buffer_view];
                    BufferResource& normal_buffer_gpu = buffers[normal_accessor.buffer_view];

                    mesh_draw.normal_buffer = normal_buffer_gpu.handle;
                    mesh_draw.normal_offset = normal_accessor.byte_offset == glTF::INVALID_INT_VALUE ? 0 : normal_accessor.byte_offset;
                } else {
                    // Compute at runtime
                    Array<vec3s> normals_array{};
                    normals_array.init(allocator, vertex_count, vertex_count);
                    memset(normals_array.data, 0, normals_array.size * sizeof(vec3s));

                    u32 index_count = mesh_draw.count;
                    for(u32 index = 0; index < index_count; index += 3) {
                        u32 i0 = indices_accessor.component_type == glTF::Accessor::UNSIGNED_INT ? index_data_32[index]  : index_data_16[index];
                        u32 i1 = indices_accessor.component_type == glTF::Accessor::UNSIGNED_INT ? index_data_32[index + 1]  : index_data_16[index + 1];
                        u32 i2 = indices_accessor.component_type == glTF::Accessor::UNSIGNED_INT ? index_data_32[index + 2]  : index_data_16[index + 2];

                        vec3s p0 = position_data[i0];
                        vec3s p1 = position_data[i1];
                        vec3s p2 = position_data[i2];

                        vec3s a = glms_vec3_sub(p1, p0);
                        vec3s b = glms_vec3_sub(p2, p0);

                        vec3s normal = glms_cross(a, b);

                        normals_array[i0] = glms_vec3_add(normals_array[i0], normal);
                        normals_array[i1] = glms_vec3_add(normals_array[i1], normal);
                        normals_array[i2] = glms_vec3_add(normals_array[i2], normal);
                    }

                    for(u32 vertex = 0; vertex < vertex_count; vertex++) {
                        normals_array[vertex] = glms_normalize(normals_array[vertex]);
                    }

                    BufferCreation normals_creation{};
                    normals_creation.set(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, ResourceUsageType::Immutable, normals_array.size * sizeof(vec3s)).set_name("normals").set_data(normals_array.data);

                    mesh_draw.normal_buffer = gpu.create_buffer(normals_creation);
                    mesh_draw.normal_offset = 0;

                    custom_mesh_buffers.push(mesh_draw.normal_buffer);

                    normals_array.shutdown();
                }

                if(tangent_accessor_index != -1) {
                    glTF::Accessor& tangent_accessor = scene.accessors[tangent_accessor_index];
                    glTF::BufferView& tangent_buffer_view = scene.buffer_views[tangent_accessor.buffer_view];
                    BufferResource& tangent_buffer_gpu = buffers[tangent_accessor.buffer_view];

                    mesh_draw.tangent_buffer = tangent_buffer_gpu.handle;
                    mesh_draw.tangent_offset = tangent_accessor.byte_offset == glTF::INVALID_INT_VALUE ? 0 : tangent_accessor.byte_offset;

                    mesh_draw.material_data.flags |= MaterialFeatures_TangentVertexAttribute;
                }

                if(texcoord_accessor_index != -1) {
                    glTF::Accessor& texcoord_accessor = scene.accessors[texcoord_accessor_index];
                    glTF::BufferView& texcoord_buffer_view = scene.buffer_views[texcoord_accessor.buffer_view];
                    BufferResource& texcoord_buffer_gpu = buffers[texcoord_accessor.buffer_view];

                    mesh_draw.texcoord_buffer = texcoord_buffer_gpu.handle;
                    mesh_draw.texcoord_offset = texcoord_accessor.byte_offset == glTF::INVALID_INT_VALUE ? 0 : texcoord_accessor.byte_offset;

                    mesh_draw.material_data.flags |= MaterialFeatures_TexcoordVertexAttribute;
                }

                PASSERTM(mesh_primitive.material != glTF::INVALID_INT_VALUE, "Mesh with no material is not supported");
                glTF::Material& material = scene.materials[mesh_primitive.material];

                // Descriptor set
                DescriptorSetCreation ds_creation{};
                ds_creation.set_layout(cube_dsl).buffer(cube_cb, 0);

                buffer_creation.reset().set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceUsageType::Dynamic, sizeof(MaterialData)).set_name("material");
                mesh_draw.material_buffer = gpu.create_buffer(buffer_creation);
                ds_creation.buffer(mesh_draw.material_buffer, 1);

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

                        SamplerHandle sampler_handle = dummy_sampler;
                        if(diffuse_texture.sampler != glTF::INVALID_INT_VALUE) {
                            sampler_handle = samplers[diffuse_texture.sampler].handle;
                        }

                        ds_creation.texture_sampler(diffuse_texture_gpu.handle, sampler_handle, 2);

                        mesh_draw.material_data.flags |= MaterialFeatures_ColorTexture;
                    } else {
                        ds_creation.texture_sampler(dummy_texture, dummy_sampler, 2);
                    }

                    if(material.pbr_metallic_roughness->metallic_roughness_texture != nullptr) {
                        glTF::Texture& roughness_texture = scene.textures[material.pbr_metallic_roughness->metallic_roughness_texture->index];
                        TextureResource& roughness_texture_gpu = images[roughness_texture.source];

                        SamplerHandle sampler_handle = dummy_sampler;
                        if(roughness_texture.sampler != glTF::INVALID_INT_VALUE) {
                            sampler_handle = samplers[roughness_texture.sampler].handle;
                        }

                        ds_creation.texture_sampler(roughness_texture_gpu.handle, sampler_handle, 3);

                        mesh_draw.material_data.flags |= MaterialFeatures_RoughnessTexture;
                    } else {
                        ds_creation.texture_sampler(dummy_texture, dummy_sampler, 3);
                    }

                    if(material.pbr_metallic_roughness->metallic_factor != glTF::INVALID_FLOAT_VALUE) {
                        mesh_draw.material_data.metallic_factor = material.pbr_metallic_roughness->metallic_factor;
                    } else {
                        mesh_draw.material_data.metallic_factor = 1.0f;
                    }

                    if(material.pbr_metallic_roughness->roughness_factor != glTF::INVALID_FLOAT_VALUE) {
                        mesh_draw.material_data.roughness_factor = material.pbr_metallic_roughness->roughness_factor;
                    } else {
                        mesh_draw.material_data.roughness_factor = 1.0f;
                    }
                }

                if(material.occlusion_texture != nullptr) {
                    glTF::Texture& occlusion_texture = scene.textures[material.occlusion_texture->index];
                    TextureResource& occlusion_texture_gpu = images[occlusion_texture.source];
                    SamplerResource& occlusion_sampler_gpu = samplers[occlusion_texture.sampler];

                    ds_creation.texture_sampler(occlusion_texture_gpu.handle, occlusion_sampler_gpu.handle, 2);
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

    for(u32 buffer_index = 0; buffer_index < scene.buffers_count; buffer_index++) {
        void* buffer = buffers_data[buffer_index];
        allocator->deallocate(buffer);
    }
    buffers_data.shutdown();

    i64 begin_frame_tick = time_now();

    vec3s eye = vec3s { 0.0f, 2.5f, 2.0f };
    vec3s look = vec3s { 0.0f, 0.0f, -1.0f };
    vec3s right = vec3s { 1.0f, 0.0f, 0.0f };

    f32 yaw = 0.0f;
    f32 pitch = 0.0f;

    float model_scale = 0.008f;

    int frame = 0;

    while(!window.should_exit()) {

        ZoneScoped;

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
                if(input_handler.is_mouse_button_down(MouseButton::MOUSE_BUTTON_LEFT) && !ImGui::GetIO().WantCaptureMouse) {

                    f32 mouse_pos_dy = (f32)(input_handler.mouse_position.y - input_handler.previous_mouse_position.y);
                    f32 mouse_pos_dx = (f32)(input_handler.mouse_position.x - input_handler.previous_mouse_position.x);

                    pitch += (mouse_pos_dy) * 0.1f;
                    yaw += (mouse_pos_dx) * 0.3f;

                    pitch = clamp(pitch, -60.0f, 60.0f);

                    if(yaw > 360.0f) {
                        yaw -= 360.0f;
                    }

                    mat3s rxm = glms_mat4_pick3( glms_rotate_make(glm_rad(-pitch), vec3s {1.0f, 0.0f, 0.0f }));
                    mat3s rym = glms_mat4_pick3(glms_rotate_make(glm_rad(-yaw), vec3s {0.0f, 1.0f, 0.0f}));

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
                    eye = glms_vec3_sub(eye, glms_vec3_scale(right, 5.0f * delta_time));
                }

                mat4s view = glms_lookat(eye, glms_vec3_add(eye, look), vec3s{0.0f, 1.0f, 0.0f});
                mat4s projection = glms_perspective(glm_rad(60.0f), gpu.swapchain_width * 1.0f / gpu.swapchain_height, 0.01f, 1000.0f);

                // Calculate view projection matrix
                mat4s view_projection = glms_mat4_mul(projection, view);

                // Rotate cube
                rx += 1.0f * delta_time;
                ry += 2.0f * delta_time;

                mat4s rxm = glms_rotate_make(rx, vec3s{1.0f, 0.0f, 0.0f});
                mat4s rym = glms_rotate_make(glm_rad(45.0f), vec3s{0.0f, 1.0f, 0.0f});

                mat4s sm = glms_scale_make(vec3s{model_scale, model_scale, model_scale});
                mat4s model = glms_mat4_mul(rym, sm);

                UniformData uniform_data{};
                uniform_data.vp = view_projection, model;
                uniform_data.m = model;
                uniform_data.inverseM = glms_mat4_inv(glms_mat4_transpose(model));
                uniform_data.eye = vec4s {eye.x, eye.y, eye.z, 1.0f};
                uniform_data.light = vec4s {2.0f, 2.0f, 0.0f, 1.0f};

                memcpy(cb_data, &uniform_data, sizeof(UniformData));

                gpu.unmap_buffer(cb_map);
            }
        }

        if(!window.minimized) {
            puffin::CommandBuffer* gpu_commands = gpu.get_command_buffer(QueueType::Graphics, true);
            gpu_commands->push_marker("Frame");

            gpu_commands->clear(0.3f, 0.9f, 0.3f, 1.0f);
            gpu_commands->clear_depth_stencil(1.0f, 0);
            gpu_commands->bind_pass(gpu.get_swapchain_pass());
            gpu_commands->bind_pipeline(cube_pipeline);
            gpu_commands->set_scissors(nullptr);
            gpu_commands->set_viewport(nullptr);

            for(u32 mesh_index = 0; mesh_index < mesh_draws.size; mesh_index++) {
                MeshDraw mesh_draw = mesh_draws[mesh_index];

                MapBufferParameters material_map = { mesh_draw.material_buffer, 0, 0 };
                MaterialData* material_buffer_data = (MaterialData*) gpu.map_buffer(material_map);

                memcpy(material_buffer_data, &mesh_draw.material_data, sizeof(MaterialData));

                gpu.unmap_buffer(material_map);

                gpu_commands->bind_vertex_buffer(mesh_draw.position_buffer, 0, mesh_draw.position_offset);
                gpu_commands->bind_vertex_buffer(mesh_draw.tangent_buffer, 1, mesh_draw.tangent_offset);
                gpu_commands->bind_vertex_buffer(mesh_draw.normal_buffer, 2, mesh_draw.normal_offset);
                gpu_commands->bind_vertex_buffer(mesh_draw.texcoord_buffer, 3, mesh_draw.texcoord_offset);
                gpu_commands->bind_index_buffer(mesh_draw.index_buffer, mesh_draw.index_offset, mesh_draw.index_type);
                gpu_commands->bind_descriptor_set(&mesh_draw.descriptor_set, 1, nullptr, 0);

                gpu_commands->draw_indexed(TopologyType::Triangle, mesh_draw.count, 1, 0, 0, 0);
            }

            imgui->render(*gpu_commands);

            gpu_commands->pop_marker();

            gpu_profiler.update(gpu);

            // Send commands to GPU
            gpu.queue_command_buffer(gpu_commands);
            gpu.present();
        } else {
            ImGui::Render();
        }

        FrameMark;

        frame++;
    }

    for(u32 mesh_index = 0; mesh_index < mesh_draws.size; mesh_index++) {
        MeshDraw& mesh_draw = mesh_draws[mesh_index];
        gpu.destroy_descriptor_set(mesh_draw.descriptor_set);
        gpu.destroy_buffer(mesh_draw.material_buffer);
    }

    mesh_draws.shutdown();

    gpu.destroy_buffer(cube_cb);
    gpu.destroy_pipeline(cube_pipeline);
    gpu.destroy_descriptor_set_layout(cube_dsl);

    imgui->shutdown();

    gpu_profiler.shutdown();

    rm.shutdown();
    renderer.shutdown();

    samplers.shutdown();
    images.shutdown();
    buffers.shutdown();

    resource_name_buffer.shutdown();

    gltf_free(scene);

    input_handler.shutdown();
    window.shutdown();

    MemoryService::instance()->shutdown();

    return 0;

}