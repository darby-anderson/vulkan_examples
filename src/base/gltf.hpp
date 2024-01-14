//
// Created by darby on 12/9/2023.
//

#pragma once

#include "memory.hpp"
#include "platform.hpp"
#include "string.hpp"
#include "file_system.hpp"

static const char* kDefault3DModel = "../../models/gltf/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf";

#define InjectDefault3DModel() \
    if(puffin::file_exists(kDefault3DModel)) { \
        argc = 2;              \
        argv[1] = const_cast<char*>(kDefault3DModel); \
    }                          \
    else {                     \
        exit(-1); \
    }                          \

namespace puffin {
namespace glTF {

static const i32 INVALID_INT_VALUE = 2147483647;
static_assert(INVALID_INT_VALUE == i32_max, "Mismatch between invalid int and i32_max");
static const f32 INVALID_FLOAT_VALUE = 3.402823466e+38F;

struct Asset {
    StringBuffer            copyright;
    StringBuffer            generator;
    StringBuffer            minVersion;
    StringBuffer            version;
};

struct CameraOrthographic {
    f32                     xmag;
    f32                     ymag;
    f32                     zfar;
    f32                     znear;
};

struct AccessorSparse {
    i32                     count;
    i32                     indices;
    i32                     values;
};

struct Camera {
    i32                     orthographic;
    i32                     perspective;
    StringBuffer            type;
};

struct AnimationChannel {
    enum TargetType {
        Translation, Rotation, Scale, Weights, Count
    };

    i32                     sampler;
    i32                     target_node;
    TargetType              target_type;
};

struct AnimationSampler {
    i32                     input_keyframe_buffer_index;
    i32                     output_keyframe_buffer_index;

    enum Interpolation {
        Linear, Step, CubicSpline, Count
    };

    Interpolation            interpolation;
};

struct Skin {
    i32                     inverse_bind_matrices_buffer_index;
    i32                     skeleton_root_node_index;
    u32                     joints_count;
    i32*                    joints;
};

struct BufferView {
    enum Target {
        ARRAY_BUFFER = 34962, /* Vertex Data */ ELEMENT_ARRAY_BUFFER = 34963 /* Index Data */
    };

    i32                     buffer;
    i32                     byte_length;
    i32                     byte_offset;
    i32                     byte_stride;
    i32                     target;
    StringBuffer            name;
};

struct Image {
    i32                     buffer_view;
    StringBuffer            mime_type;
    StringBuffer            uri;
};

struct Node {
    i32                     camera;
    u32                     children_count;
    i32*                    children;
    u32                     matrix_count;
    f32*                    matrix;
    i32                     mesh;
    u32                     rotation_count;
    f32*                    rotation;
    u32                     scale_count;
    f32*                    scale;
    i32                     skin;
    u32                     translation_count;
    f32*                    translation;
    u32                     weights_count;
    f32*                    weights;
    StringBuffer            name;
};

struct TextureInfo {
    i32                     index;
    i32                     texCoord;
};

struct MaterialPBRMetallicRoughness {
    u32                     base_color_factor_count;
    f32*                    base_color_factor;
    TextureInfo*            base_color_texture;
    f32                     metallic_factor;
    TextureInfo*            metallic_roughness_texture;
    f32                     roughness_factor;
};

struct MeshPrimitive {
    struct Attribute {
        StringBuffer        key;
        i32                 accessor_index;
    };

    u32                     attribute_count;
    Attribute*              attributes;
    i32                     indices;
    i32                     material;

    // 0 points
    // 1 lines
    // 2 line_loop
    // 3 line_strip
    // 4 triangles
    // 5 triangle_strip
    // 6 triangle_fan
    i32                     mode;
};

struct AccessorSparseIndices {
    i32                     buffer_view;
    i32                     byte_offset;
    // 5121 unsigned_byte
    // 5123 unsigned_short
    // 5125 unsigned_int
    i32                     component_type;
};

struct Accessor {
    enum ComponentType {
        BYTE = 5120, UNSIGNED_BYTE = 5121, SHORT = 5122, UNSIGNED_SHORT = 5123, UNSIGNED_INT = 5125, FLOAT = 5126
    };

    enum Type {
        Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4
    };

    i32                     buffer_view;
    i32                     byte_offset;

    i32                     component_type;
    i32                     count;
    u32                     max_count;
    f32*                    max;
    u32                     min_count;
    f32*                    min;
    bool                    normalized;
    i32                     sparse;
    Type                    type;
};

struct Texture {
    i32                     sampler;
    i32                     source;
    StringBuffer            name;
};

struct MaterialNormalTextureInfo {
    i32                     index;
    i32                     tex_coord;
    f32                     scale;
};

struct Mesh {
    u32                     primitives_count;
    MeshPrimitive*          primitives;
    u32                     weights_count;
    f32*                    weights;
    StringBuffer            name;
};

struct MaterialOcclusionTextureInfo {
    i32                     index;
    i32                     texCoord;
    f32                     strength;
};

struct Material {
    f32                     alpha_cutoff;

    StringBuffer            alpha_mode;
    bool                    double_sided;
    u32                     emissive_factor_count;
    f32*                    emissive_factor;
    TextureInfo*            emissive_texture;
    MaterialNormalTextureInfo*      normal_texture;
    MaterialOcclusionTextureInfo*   occlusion_texture;
    MaterialPBRMetallicRoughness*   pbr_metallic_roughness;
    StringBuffer            name;
};

struct Buffer {
    i32                     byte_length;
    StringBuffer            uri;
    StringBuffer            name;
};

struct CameraPerspective {
    f32                     aspect_ratio;
    f32                     yfov;
    f32                     zfar;
    f32                     znear;
};

struct Animation {
    u32                     channels_count;
    AnimationChannel*       channels;
    u32                     samplers_count;
    AnimationSampler*       samplers;
};

struct AccessorSparseValues {
    i32                     bufferView;
    i32                     byteOffset;
};

struct Scene {
    u32                     nodes_count;
    i32*                    nodes;
};

struct Sampler {
    enum Filter {
        NEAREST = 9728, LINEAR = 9729, NEAREST_MIPMAP_NEAREST = 9984, LINEAR_MIPMAP_NEAREST = 9985, NEAREST_MIPMAP_LINEAR = 9986, LINEAR_MIPMAP_LINEAR = 9987
    };

    enum Wrap {
        CLAMP_TO_EDGE = 33071, MIRRORED_REPEAT = 33648, REPEAT = 10497
    };

    i32                     mag_filter;
    i32                     min_filter;
    i32                     wrap_s;
    i32                     wrap_t;
};

struct glTF {
    u32                     accessors_count;
    Accessor*               accessors;
    u32                     animations_count;
    Animation*              animations;
    Asset                   asset;
    u32                     buffer_views_count;
    BufferView*             buffer_views;
    u32                     buffers_count;
    Buffer*                 buffers;
    u32                     cameras_count;
    Camera*                 cameras;
    u32                     extensions_required_count;
    StringBuffer*           extensions_required;
    u32                     extensions_used_count;
    StringBuffer*           extensions_used;
    u32                     images_count;
    Image*                  images;
    u32                     materials_count;
    Material*               materials;
    u32                     meshes_count;
    Mesh*                   meshes;
    u32                     nodes_count;
    Node*                   nodes;
    u32                     samplers_count;
    Sampler*                samplers;
    i32                     scene;
    u32                     scene_count;
    Scene*                  scenes;
    u32                     skins_count;
    Skin*                   skins;
    u32                     textures_count;
    Texture*                textures;

    LinearAllocator         allocator;
};


i32                 get_data_offset(i32 accessor_offset, i32 buffer_view_offset);

} // glTF

glTF::glTF          gltf_load_file(cstring file_path);

void                gltf_free( glTF::glTF& scene );

i32                 gltf_get_attribute_accessor_index(glTF::MeshPrimitive::Attribute* attributes, u32 attributes_count, cstring attribute_name);

} // puffin
