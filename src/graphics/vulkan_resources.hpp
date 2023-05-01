//
// Created by darby on 2/26/2023.
//
#pragma once

#include "vulkan/vulkan.h"

#include "gpu_enum.hpp"
#include "platform.hpp"

static const u32                k_invalid_index = 0xffffffff;

typedef u32                     ResourceHandle;

// Consts
static const u8                 k_max_image_outputs             = 8; // Maximum number of images/render_targets/fbo attachments usable
static const u8                 k_max_shader_stages             = 5; // Maximum simultaneous shader stages. Applicable to all different types of pipelines.
static const u8                 k_max_descriptor_set_layouts    = 8; // Maximum number of layouts in the pipeline.
static const u8                 k_max_descriptors_per_set       = 16;
static const u8                 k_max_vertex_streams            = 16;
static const u8                 k_max_vertex_attributes         = 16;

// Typed resource handles
struct BufferHandle {
    ResourceHandle              index;
};

struct TextureHandle {
    ResourceHandle              index;
};

struct PipelineHandle {
    ResourceHandle              index;
};

struct SamplerHandle {
    ResourceHandle              index;
};

struct DescriptorSetLayoutHandle {
    ResourceHandle              index;
};

struct DescriptorSetHandle {
    ResourceHandle              index;
};

struct RenderPassHandle {
    ResourceHandle              index;
};

struct ShaderStateHandle {
    ResourceHandle              index;
};

// Invalid Handles
static BufferHandle                 k_invalid_buffer    { k_invalid_index };
static TextureHandle                k_invalid_texture   { k_invalid_index };
static ShaderStateHandle            k_invalid_shader    { k_invalid_index };
static SamplerHandle                k_invalid_sampler   { k_invalid_index };
static DescriptorSetLayoutHandle    k_invalid_layout    { k_invalid_index };
static DescriptorSetHandle          k_invalid_set       { k_invalid_index };
static PipelineHandle               k_invalid_pipeline  { k_invalid_index };
static RenderPassHandle             k_invalid_pass      { k_invalid_index };

// Creation Datas
// Buffers in Vulkan hold data to be used by the GPU -> typically best for larger sets like indices and vertices
struct BufferCreation {

    VkBufferUsageFlags          type_flags = 0;
    ResourceUsageType::Enum     usage = ResourceUsageType::Immutable;
    u32                         size = 0;
    void*                       initialData = nullptr;

    const char*                 name = "nullptr";

    BufferCreation&             reset();
    BufferCreation&             set(VkBufferUsageFlags flags, ResourceUsageType::Enum usage, u32 size);
    BufferCreation&             set_data( void* data);
    BufferCreation&             set_name(const char* name);
};

struct TextureCreation {
    void*                       initial_data = nullptr;
    u16                         width       = 1;
    u16                         height      = 1;
    u16                         depth       = 1;
    u8                          mipmaps     = 1;
    u8                          flags       = 0; // TextureFlags bitmasks

    VkFormat                    format      = VK_FORMAT_UNDEFINED;
    TextureType::Enum           type        = TextureType::Texture2D;

    const char*                 name        = nullptr;

    TextureCreation&            set_size(u16 width, u16 height, u16 depth);
    TextureCreation&            set_flags(u8 mipmaps, u8 flags);
    TextureCreation&            set_format_type(VkFormat format, TextureType::Enum type);
    TextureCreation&            set_name(const char* name);
    TextureCreation&            set_data(void* data);
};

// A sampler is a distinct object that provides an interface to extract colors from a texture
struct SamplerCreation {
    VkFilter                    min_filter = VK_FILTER_NEAREST; // How minified texels should be interpolated
    VkFilter                    mag_filter = VK_FILTER_NEAREST; // How magnified texels should be interpolated
    VkSamplerMipmapMode         mip_filter = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    VkSamplerAddressMode        address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode        address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode        address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    const char*                 name = nullptr;

    SamplerCreation&            set_min_mag_mip( VkFilter min, VkFilter mag, VkSamplerMipmapMode mip );
    SamplerCreation&            set_address_mode_u( VkSamplerAddressMode u );
    SamplerCreation&            set_address_mode_uv( VkSamplerAddressMode u, VkSamplerAddressMode v );
    SamplerCreation&            set_address_mode_uvw( VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w );
    SamplerCreation&            set_name( const char* name );
};

struct ShaderStage {
    const char*                 code = nullptr;
    u32                         code_size = 0;
    VkShaderStageFlagBits       type = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
};


struct ShaderStateCreation {
    ShaderStage                 stages[k_max_shader_stages];

    const char*                 name            = nullptr;

    u32                         stages_count    = 0;
    u32                         spv_input       = 0;

    // Building helpers
    ShaderStateCreation&        reset();
    ShaderStateCreation&        set_name(const char* name);
    ShaderStateCreation&        add_stage(const char* code, u32 code_size, VkShaderStageFlagBits type);
    ShaderStateCreation&        set_spv_input(bool value);

};

struct DescriptorSetLayoutCreation {

    // A single descriptor binding. It can be relative to one or more resources of the same type.
    struct Binding {
        VkDescriptorType        type    = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        u16                     start   = 0;
        u16                     count   = 0;
        const char*             name    = nullptr;
    };

    Binding                     bindings[k_max_descriptors_per_set];
    u32                         num_bindings    = 0;
    u32                         set_index       = 0;

    const char*                 name = nullptr;

    // Building helpers
    DescriptorSetLayoutCreation&    reset();
    DescriptorSetLayoutCreation&    add_binding( const Binding& binding );
    // DescriptorSetLayoutCreation&    add_binding_at_index( const Binding& binding, int index ); DID I ADD THIS MYSELF??
    DescriptorSetLayoutCreation&    set_name( cstring name );
    DescriptorSetLayoutCreation&    set_set_index( u32 index );
};

struct DescriptorSetCreation {

    ResourceHandle              resources[k_max_descriptors_per_set];
    SamplerHandle               samplers[k_max_descriptors_per_set];
    u16                         bindings[k_max_descriptors_per_set];

    DescriptorSetLayoutHandle   layout;
    u32                         num_resources = 0;

    const char*                 name = nullptr;

    // Building helpers
    DescriptorSetCreation&      reset();
    DescriptorSetCreation&      set_layout( DescriptorSetLayoutHandle layout );
    DescriptorSetCreation&      texture( TextureHandle texture, u16 binding );
    DescriptorSetCreation&      buffer( BufferHandle buffer, u16 binding );
    DescriptorSetCreation&      texture_sampler( TextureHandle texture, SamplerHandle sampler, u16 binding );
    DescriptorSetCreation&      set_name( const char* name );
};

// Descriptor Set Update
struct DescriptorSetUpdate {
    DescriptorSetHandle         descriptor_set;

    u32                         frame_issued = 0;
};

// Vertex Attribute
struct VertexAttribute {
    u16                         location = 0;
    u16                         binding = 0;
    u32                         offset = 0;

    VertexComponentFormat::Enum format = VertexComponentFormat::Count;
};

// Vertex Stream
struct VertexStream {
    u16                         binding = 0;
    u16                         stride = 0;
    VertexInputRate::Enum       input_rate = VertexInputRate::Count;
};

struct VertexInputCreation {
    u32                         num_vertex_streams = 0;
    u32                         num_vertex_attributes = 0;

    VertexStream                vertex_streams[k_max_vertex_streams];
    VertexAttribute             vertex_attributes[k_max_vertex_attributes];

    VertexInputCreation&        reset();
    VertexInputCreation&        add_vertex_stream(const VertexStream& stream);
    VertexInputCreation&        add_vertex_stream(const VertexAttribute& attribute);

};

struct RenderPassOutput {
    VkFormat                    color_formats[k_max_image_outputs];
    VkFormat                    depth_format;
    u32                         num_color_formats;

    RenderPassOperation::Enum   color_operation = RenderPassOperation::DontCare;
    RenderPassOperation::Enum   depth_operation = RenderPassOperation::DontCare;
    RenderPassOperation::Enum   stencil_operation = RenderPassOperation::DontCare;

    RenderPassOutput&           reset();
    RenderPassOutput&           color(VkFormat format);
    RenderPassOutput&           depth(VkFormat format);
    RenderPassOutput&           set_operations(VkFormat format);
};

struct RenderPassCreation {
    u16                         num_render_targets  = 0;
    RenderPassType::Enum        type                = RenderPassType::Geometry;

    TextureHandle               output_textures[k_max_image_outputs];
    TextureHandle               depth_stencil_texture;

    f32                         scale_x             = 1.f;
    f32                         scale_y             = 1.f;
    u8                          resize              = 1;

    RenderPassOperation::Enum   color_operation     = RenderPassOperation::DontCare;
    RenderPassOperation::Enum   depth_operation     = RenderPassOperation::DontCare;
    RenderPassOperation::Enum   stencil_operation   = RenderPassOperation::DontCare;

    const char*                 name = nullptr;

    RenderPassCreation&         reset();
    RenderPassCreation&         add_render_texture( TextureHandle texture );
    RenderPassCreation&         set_scaling( f32 scale_x, f32 scale_y, u8 resize );
    RenderPassCreation&         set_depth_stencil_texture( TextureHandle texture );
    RenderPassCreation&         set_name( const char* name );
    RenderPassCreation&         set_type( RenderPassType::Enum type );
    RenderPassCreation&         set_operations( RenderPassOperation::Enum color, RenderPassOperation::Enum depth, RenderPassOperation::Enum stencil );
};

//
/*
struct PipelineCreation {

    RasterizationCreation       rasterization; // START HERE!!!!
    DepthStencilCreation        depth_stencil;
    BlendStateCreation          blend_state;
    VertexInputCreation         vertex_input;
    ShaderStateCreation         shader_state;

    RenderPassOutput            render_pass;
    DescriptorSetLayoutHandle   descriptor_set_layout[k_max_descriptor_set_layouts];
    const ViewportState*        viewport = nullptr;

    u32                         num_active_layouts = 0;

    const char*                 name = nullptr;

    PipelineCreation&           add_descriptor_set_layout(DescriptorSetLayoutHandle handle);
    RenderPassOutput&           render_pass_output();
};*/
