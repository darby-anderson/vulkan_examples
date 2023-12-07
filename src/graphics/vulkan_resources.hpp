//
// Created by darby on 2/26/2023.
//
#pragma once

#include "vk_mem_alloc.h"

#include "gpu_enum.hpp"
#include "platform.hpp"

namespace puffin {

static const u32 k_invalid_index = 0xffffffff;

typedef u32 ResourceHandle;

// Consts
static const u8 k_max_image_outputs = 8; // Maximum number of images/render_targets/fbo attachments usable
static const u8 k_max_shader_stages = 5; // Maximum simultaneous shader stages. Applicable to all different types of pipelines.
static const u8 k_max_descriptor_set_layouts = 8; // Maximum number of layouts in the pipeline.
static const u8 k_max_descriptors_per_set = 16;
static const u8 k_max_vertex_streams = 16;
static const u8 k_max_vertex_attributes = 16;

// Typed resource handles
struct BufferHandle {
    ResourceHandle index;
};

struct TextureHandle {
    ResourceHandle index;
};

struct PipelineHandle {
    ResourceHandle index;
};

struct SamplerHandle {
    ResourceHandle index;
};

struct DescriptorSetLayoutHandle {
    ResourceHandle index;
};

struct DescriptorSetHandle {
    ResourceHandle index;
};

struct RenderPassHandle {
    ResourceHandle index;
};

struct ShaderStateHandle {
    ResourceHandle index;
};

// Invalid Handles
static BufferHandle k_invalid_buffer{k_invalid_index};
static TextureHandle k_invalid_texture{k_invalid_index};
static ShaderStateHandle k_invalid_shader{k_invalid_index};
static SamplerHandle k_invalid_sampler{k_invalid_index};
static DescriptorSetLayoutHandle k_invalid_layout{k_invalid_index};
static DescriptorSetHandle k_invalid_set{k_invalid_index};
static PipelineHandle k_invalid_pipeline{k_invalid_index};
static RenderPassHandle k_invalid_pass{k_invalid_index};

// Resource creation structs
struct Rect2D {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 width = 0.0f;
    f32 height = 0.0f;
};

struct Rect2DInt {
    i16 x = 0;
    i16 y = 0;
    u16 width = 0;
    u16 height = 0;
};

struct Viewport {
    Rect2DInt rect;
    f32 min_depth = 0.0f;
    f32 max_depth = 0.0f;
};

struct ViewportState {
    u32 num_viewports = 0;
    u32 num_scissors = 0;

    Viewport* viewport = nullptr;
    Rect2DInt* scissors = nullptr;
};

struct StencilOperationState {
    VkStencilOp fail = VK_STENCIL_OP_KEEP;
    VkStencilOp pass = VK_STENCIL_OP_KEEP;
    VkStencilOp depth_fail = VK_STENCIL_OP_KEEP;
    VkCompareOp compare = VK_COMPARE_OP_ALWAYS;

    u32 compare_mask = 0xff;
    u32 write_mask = 0xff;
    u32 reference = 0xff;
};

struct DepthStencilCreation {
    StencilOperationState front;
    StencilOperationState back;
    VkCompareOp depth_comparison = VK_COMPARE_OP_ALWAYS;

    u8 depth_enable = 1;
    u8 depth_write_enable = 1;
    u8 stencil_enable = 1;
    u8 pad = 5;

    DepthStencilCreation() : depth_enable(0), depth_write_enable(0), stencil_enable(0) {}

    DepthStencilCreation& set_depth(bool write, VkCompareOp comparison_test);
};

// Buffers in Vulkan hold data to be used by the GPU -> typically best for larger sets like indices and vertices
struct BufferCreation {

    VkBufferUsageFlags type_flags = 0;
    ResourceUsageType::Enum usage = ResourceUsageType::Immutable;
    u32 size = 0;
    void* initial_data = nullptr;

    const char* name = "nullptr";

    BufferCreation& reset();

    BufferCreation& set(VkBufferUsageFlags flags, ResourceUsageType::Enum usage, u32 size);

    BufferCreation& set_data(void* data);

    BufferCreation& set_name(const char* name);
};

struct TextureCreation {
    void* initial_data = nullptr;
    u16 width = 1;
    u16 height = 1;
    u16 depth = 1;
    u8 mipmaps = 1;
    u8 flags = 0; // TextureFlags bitmasks

    VkFormat format = VK_FORMAT_UNDEFINED;
    TextureType::Enum type = TextureType::Texture2D;

    const char* name = nullptr;

    TextureCreation& set_size(u16 width, u16 height, u16 depth);

    TextureCreation& set_flags(u8 mipmaps, u8 flags);

    TextureCreation& set_format_type(VkFormat format, TextureType::Enum type);

    TextureCreation& set_name(const char* name);

    TextureCreation& set_data(void* data);
};

// A sampler is a distinct object that provides an interface to extract colors from a texture
struct SamplerCreation {
    VkFilter min_filter = VK_FILTER_NEAREST; // How minified texels should be interpolated
    VkFilter mag_filter = VK_FILTER_NEAREST; // How magnified texels should be interpolated
    VkSamplerMipmapMode mip_filter = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    const char* name = nullptr;

    SamplerCreation& set_min_mag_mip(VkFilter min, VkFilter mag, VkSamplerMipmapMode mip);

    SamplerCreation& set_address_mode_u(VkSamplerAddressMode u);

    SamplerCreation& set_address_mode_uv(VkSamplerAddressMode u, VkSamplerAddressMode v);

    SamplerCreation& set_address_mode_uvw(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w);

    SamplerCreation& set_name(const char* name);
};

struct ShaderStage {
    const char* code = nullptr;
    u32 code_size = 0;
    VkShaderStageFlagBits type = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
};


struct ShaderStateCreation {
    ShaderStage stages[k_max_shader_stages];

    const char* name = nullptr;

    u32 stages_count = 0;
    u32 spv_input = 0;

    // Building helpers
    ShaderStateCreation& reset();

    ShaderStateCreation& set_name(const char* name);

    ShaderStateCreation& add_stage(const char* code, u32 code_size, VkShaderStageFlagBits type);

    ShaderStateCreation& set_spv_input(bool value);

};

struct DescriptorSetLayoutCreation {

    // A single descriptor binding. It can be relative to one or more resources of the same type.
    struct Binding {
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        u16 start = 0;
        u16 count = 0;
        const char* name = nullptr;
    };

    Binding bindings[k_max_descriptors_per_set];
    u32 num_bindings = 0;
    u32 set_index = 0;

    const char* name = nullptr;

    // Building helpers
    DescriptorSetLayoutCreation& reset();

    DescriptorSetLayoutCreation& add_binding(const Binding& binding);

    // DescriptorSetLayoutCreation&    add_binding_at_index( const Binding& binding, int index ); DID I ADD THIS MYSELF??
    DescriptorSetLayoutCreation& set_name(cstring name);

    DescriptorSetLayoutCreation& set_set_index(u32 index);
};

struct DescriptorSetCreation {

    ResourceHandle resources[k_max_descriptors_per_set];
    SamplerHandle samplers[k_max_descriptors_per_set];
    u16 bindings[k_max_descriptors_per_set];

    DescriptorSetLayoutHandle layout;
    u32 num_resources = 0;

    const char* name = nullptr;

    // Building helpers
    DescriptorSetCreation& reset();

    DescriptorSetCreation& set_layout(DescriptorSetLayoutHandle layout);

    DescriptorSetCreation& texture(TextureHandle texture, u16 binding);

    DescriptorSetCreation& buffer(BufferHandle buffer, u16 binding);

    DescriptorSetCreation& texture_sampler(TextureHandle texture, SamplerHandle sampler, u16 binding);

    DescriptorSetCreation& set_name(const char* name);
};

// Descriptor Set Update
struct DescriptorSetUpdate {
    DescriptorSetHandle descriptor_set;

    u32 frame_issued = 0;
};

// Vertex Attribute
struct VertexAttribute {
    u16 location = 0;
    u16 binding = 0;
    u32 offset = 0;

    VertexComponentFormat::Enum format = VertexComponentFormat::Count;
};

// Vertex Stream
struct VertexStream {
    u16 binding = 0;
    u16 stride = 0;
    VertexInputRate::Enum input_rate = VertexInputRate::Count;
};

struct VertexInputCreation {
    u32 num_vertex_streams = 0;
    u32 num_vertex_attributes = 0;

    VertexStream vertex_streams[k_max_vertex_streams];
    VertexAttribute vertex_attributes[k_max_vertex_attributes];

    VertexInputCreation& reset();
    VertexInputCreation& add_vertex_stream(const VertexStream& stream);
    VertexInputCreation& add_vertex_attribute(const VertexAttribute& attribute);

};

struct RenderPassOutput {
    VkFormat color_formats[k_max_image_outputs];
    VkFormat depth_stencil_format;
    u32 num_color_formats;

    RenderPassOperation::Enum color_operation = RenderPassOperation::DontCare;
    RenderPassOperation::Enum depth_operation = RenderPassOperation::DontCare;
    RenderPassOperation::Enum stencil_operation = RenderPassOperation::DontCare;

    RenderPassOutput& reset();

    RenderPassOutput& color(VkFormat format);

    RenderPassOutput& depth(VkFormat format);

    RenderPassOutput&
    set_operations(RenderPassOperation::Enum color, RenderPassOperation::Enum depth, RenderPassOperation::Enum stencil);
};

struct RenderPassCreation {
    u16 num_render_targets = 0;
    RenderPassType::Enum type = RenderPassType::Geometry;

    TextureHandle output_textures[k_max_image_outputs];
    TextureHandle depth_stencil_texture;

    f32 scale_x = 1.f;
    f32 scale_y = 1.f;
    u8 resize = 1;

    RenderPassOperation::Enum color_operation = RenderPassOperation::DontCare;
    RenderPassOperation::Enum depth_operation = RenderPassOperation::DontCare;
    RenderPassOperation::Enum stencil_operation = RenderPassOperation::DontCare;

    const char* name = nullptr;

    RenderPassCreation& reset();

    RenderPassCreation& add_render_texture(TextureHandle texture);

    RenderPassCreation& set_scaling(f32 scale_x, f32 scale_y, u8 resize);

    RenderPassCreation& set_depth_stencil_texture(TextureHandle texture);

    RenderPassCreation& set_name(const char* name);

    RenderPassCreation& set_type(RenderPassType::Enum type);

    RenderPassCreation&
    set_operations(RenderPassOperation::Enum color, RenderPassOperation::Enum depth, RenderPassOperation::Enum stencil);
};

struct RasterizationCreation {
    VkCullModeFlagBits          cull_mode   = VK_CULL_MODE_NONE;
    VkFrontFace                 front       = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    FillMode::Enum              fill        = FillMode::Solid;
};

struct BlendState {
    VkBlendFactor               source_color        = VK_BLEND_FACTOR_ONE;
    VkBlendFactor               destination_color   = VK_BLEND_FACTOR_ONE;
    VkBlendOp                   color_operation     = VK_BLEND_OP_ADD;

    VkBlendFactor               source_alpha        = VK_BLEND_FACTOR_ONE;
    VkBlendFactor               destination_alpha   = VK_BLEND_FACTOR_ONE;
    VkBlendOp                   alpha_operation     = VK_BLEND_OP_ADD;

    ColorWriteEnabled::Mask     color_write_mask    = ColorWriteEnabled::All_mask;

    u8                          blend_enabled       : 1;
    u8                          separate_blend      : 1;
    u8                          pad                 : 6;

    BlendState() : blend_enabled(0), separate_blend(0) {}

    BlendState&                 set_color(VkBlendFactor source_color, VkBlendFactor destination_color, VkBlendOp color_operation);
    BlendState&                 set_alpha(VkBlendFactor source_alpha, VkBlendFactor destination_alpha, VkBlendOp alpha_operation);
    BlendState&                 set_color_write_state(ColorWriteEnabled::Mask value);
};

struct BlendStateCreation {
    BlendState                  blend_states[k_max_image_outputs];
    u32                         active_states    = 0;

    BlendStateCreation&         reset();
    BlendState&                 add_blend_state();
};


struct PipelineCreation {

    RasterizationCreation rasterization;
    DepthStencilCreation depth_stencil;
    BlendStateCreation blend_state;
    VertexInputCreation vertex_input;
    ShaderStateCreation shaders;

    RenderPassOutput render_pass;
    DescriptorSetLayoutHandle descriptor_set_layout[k_max_descriptor_set_layouts];
    const ViewportState* viewport = nullptr;

    u32 num_active_layouts = 0;

    const char* name = nullptr;

    PipelineCreation& add_descriptor_set_layout(DescriptorSetLayoutHandle handle);

    RenderPassOutput& render_pass_output();
};

namespace TextureFormat {

inline bool is_depth_stencil(VkFormat value) {
    return value == VK_FORMAT_D16_UNORM_S8_UINT || value == VK_FORMAT_D24_UNORM_S8_UINT ||
           value == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

inline bool is_depth_only(VkFormat value) {
    return value >= VK_FORMAT_D16_UNORM && value < VK_FORMAT_D32_SFLOAT;
}

inline bool is_stencil_only(VkFormat value) {
    return value == VK_FORMAT_S8_UINT;
}

inline bool has_depth(VkFormat value) {
    return (value >= VK_FORMAT_D16_UNORM && value < VK_FORMAT_S8_UINT) ||
           (value >= VK_FORMAT_D16_UNORM_S8_UINT && value <= VK_FORMAT_D32_SFLOAT_S8_UINT);
}

inline bool has_stencil(VkFormat value) {
    return value >= VK_FORMAT_S8_UINT && value <= VK_FORMAT_D32_SFLOAT_S8_UINT;
}

inline bool has_depth_or_stencil(VkFormat value) {
    return value >= VK_FORMAT_D16_UNORM && value <= VK_FORMAT_D32_SFLOAT_S8_UINT;
}
}

// Synchronization

struct ImageBarrier {
    TextureHandle texture;
};

struct MemoryBarrier {
    BufferHandle buffer;
};

struct ExecutionBarrier {
    PipelineStage::Enum source_pipeline_stage;
    PipelineStage::Enum destination_pipeline_stage;

    u32 new_barrier_experimental = u32_max;
    u32 load_operation = 0;

    u32 num_image_barriers;
    u32 num_memory_barriers;

    ImageBarrier image_barriers[8];
    MemoryBarrier memory_barriers[8];

    ExecutionBarrier& reset();

    ExecutionBarrier& set(PipelineStage::Enum source, PipelineStage::Enum destination);

    ExecutionBarrier& add_image_barrier(const ImageBarrier& image_barrier);

    ExecutionBarrier& add_memory_barrier(const MemoryBarrier& memory_barrier);
};

struct ResourceUpdate {
    ResourceDeletionType::Enum type;
    ResourceHandle handle;
    u32 current_frame;
};

// Resources

static const u32 k_max_swapchain_images = 3;

struct DeviceStateVulkan;

struct Buffer {
    // A higher level of abtraction from VkDeviceMemory,
    // VkBuffers represent a section of memory
    VkBuffer vk_buffer;
    VmaAllocation vma_allocation;
    // Vulkan device operates on data in device memory via memory objects, these are VkDeviceMemory
    VkDeviceMemory vk_device_memory;
    VkDeviceSize vk_device_size;

    VkBufferUsageFlags type_flags = 0;
    ResourceUsageType::Enum usage = ResourceUsageType::Immutable;
    u32 size = 0;
    u32 global_offset = 0;

    BufferHandle handle;
    BufferHandle parent_buffer;

    cstring name = nullptr;
};

struct Sampler {
    // Defines how to filter and wrap the texture data when sampling it
    // during the rendering process
    VkSampler vk_sampler;

    // Mode used by samplers when sampling texture data
    // NEAREST - nearest neighbor -> faster
    // LINEAR - linear interpolation, smoother -> slower
    VkFilter min_filter = VK_FILTER_NEAREST;
    VkFilter mag_filter = VK_FILTER_NEAREST;
    VkSamplerMipmapMode mip_filter = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    cstring name = nullptr;
};

struct Texture {
    VkImage vk_image;
    VkImageView vk_image_view;
    VkFormat vk_format;
    VkImageLayout vk_image_layout;
    VmaAllocation vma_allocation;

    u16 width = 1;
    u16 height = 1;
    u16 depth = 1;
    u8 mipmaps = 0;
    u8 flags = 0;

    TextureHandle handle;
    TextureType::Enum type = TextureType::Texture2D;

    Sampler* sampler = nullptr;

    cstring name = nullptr;
};

struct ShaderState {
    VkPipelineShaderStageCreateInfo shader_stage_info[k_max_shader_stages];

    cstring name = nullptr;

    u32 active_shaders = 0;
    bool graphics_pipeline = false;
};

struct DescriptorBinding {
    // A descriptor is a reference to a resource, VkDescriptorType is an Enum
    // Examples: Sampler, Images, Buffer
    VkDescriptorType type;
    u16 start = 0;
    u16 count = 0;
    u16 set = 0;

    cstring name = nullptr;
};

struct DescriptorSetLayout {
    // Describes the type and order of each descriptor in the set
    // as well as the binding point for the set.
    // Created with vkAllocateDescriptorSet and updated with vkUpdateDescriptorSet
    // Bound in the pipeline via vkCmdBindDescriptorSets
    VkDescriptorSetLayout vk_descriptor_set_layout;

    VkDescriptorSetLayoutBinding* vk_binding = nullptr;
    DescriptorBinding* bindings = nullptr;
    u16 num_bindings = 0;
    u16 set_index = 0;

    DescriptorSetLayoutHandle handle;
};

struct DescriptorSet {
    // Set of all resources used by a shader
    VkDescriptorSet vk_descriptor_set;

    // each resource handle represents a resource that is bound to a descriptor slot in the set
    ResourceHandle* resources = nullptr;
    // each sampler handle represents a sampler that is bound to a desriptor slot in the set
    SamplerHandle* samplers = nullptr;
    // represents the index of the descriptor slot in the descriptor set, that corresponds to a resource or sampler
    u16* bindings = nullptr;

    const DescriptorSetLayout* layout = nullptr;
    u32 num_resources = 0;
};

struct Pipeline {
    // Represents a graphics or compute pipeline
    VkPipeline vk_pipeline;
    // Describes the uniforms, push constants and descriptor set layouts used by the pipeline
    VkPipelineLayout vk_pipeline_layout;

    // GRAPHICS vs. COMPUTE pipeline
    VkPipelineBindPoint vk_bind_point;

    ShaderStateHandle shader_state;

    const DescriptorSetLayout* descriptor_set_layout[k_max_descriptor_set_layouts];
    DescriptorSetLayoutHandle descriptor_set_layout_handle[k_max_descriptor_set_layouts];
    u32 num_active_layouts = 0;

    DepthStencilCreation depth_stencil;
    BlendStateCreation blend_state;
    RasterizationCreation rasterization;

    PipelineHandle handle;
    bool graphics_pipeline = true;
};

struct RenderPass {
    VkRenderPass vk_render_pass;
    VkFramebuffer vk_framebuffer;

    RenderPassOutput output;

    TextureHandle output_textures[k_max_image_outputs];
    TextureHandle output_depth;

    RenderPassType::Enum type;

    f32 scale_x = 1.f;
    f32 scale_y = 1.f;
    u16 width = 0;
    u16 height = 0;
    u16 dispatch_x = 0;
    u16 dispatch_y = 0;
    u16 dispatch_z = 0;

    u8 resize = 0;
    u8 num_render_targets = 0;

    cstring name = nullptr;
};

struct ResourceData {
    void* data = nullptr;
};

struct ResourceBinding {
    u16 type = 0;
    u16 start = 0;
    u16 count = 0;
    u16 set = 0;

    cstring name = nullptr;
};

// API-agnostic descriptions

struct ShaderStateDescription {
    void* native_handle = nullptr;
    cstring name = nullptr;
};

struct BufferDescription {
    void* native_handle = nullptr;
    cstring name = nullptr;

    VkBufferUsageFlags type_flags = 0;
    ResourceUsageType::Enum usage = ResourceUsageType::Immutable;
    u32 size = 0;
    BufferHandle parent_handle;
};

struct TextureDescription {
    void* native_handle = nullptr;
    cstring name = nullptr;

    u16 width = 1;
    u16 height = 1;
    u16 depth = 1;
    u8 mipmaps = 1;
    u8 render_target = 0;
    u8 compute_access = 0;

    VkFormat format = VK_FORMAT_UNDEFINED;
    TextureType::Enum type = TextureType::Texture2D;
};

struct SamplerDescription {
    cstring name = nullptr;

    VkFilter min_filter = VK_FILTER_NEAREST;
    VkFilter mag_filter = VK_FILTER_NEAREST;
    VkSamplerMipmapMode mip_filter = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;
};

struct DescriptorSetLayoutDescription {
    ResourceBinding bindings[k_max_descriptors_per_set];
    u32 num_active_bindings = 0;
};

struct DescriptorSetDescription {
    ResourceData resources[k_max_descriptors_per_set];
    u32 num_active_resources = 0;
};

struct PipelineDescription {
    ShaderStateHandle shader;
};

struct MapBufferParameters {
    BufferHandle buffer;
    u32 offset = 0;
    u32 size = 0;
};

static VkImageType to_vk_image_type(TextureType::Enum type) {
    static VkImageType s_vk_target[TextureType::Count] = {
            VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D,
            VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D
    };
    return s_vk_target[type];
}

static VkImageViewType to_vk_image_view_type(TextureType::Enum type) {
    static VkImageViewType s_vk_data[] = {
            VK_IMAGE_VIEW_TYPE_1D, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_VIEW_TYPE_3D,
            VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
    };
    return s_vk_data[type];
}

static VkFormat to_vk_vertex_format(VertexComponentFormat::Enum value) {
    // Float, Float2, Float3, Float4, Mat4, Byte, Byte4N, UByte, UByte4N, Short2, Short4, Short4N, Uint, Uint2, Uint4, Count
    static VkFormat s_vk_vertex_formats[VertexComponentFormat::Count] = {
            VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_FORMAT_R8_SINT, VK_FORMAT_R8G8B8A8_SNORM, VK_FORMAT_R8_UINT, VK_FORMAT_R8G8B8A8_UINT,
            VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16_SNORM,
            VK_FORMAT_R16G16B16A16_SINT, VK_FORMAT_R16G16B16A16_SNORM, VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT,
            VK_FORMAT_R32G32B32A32_UINT
    };
    return s_vk_vertex_formats[value];
}


static VkPipelineStageFlags to_vk_pipeline_stage(PipelineStage::Enum value) {
    static VkPipelineStageFlags s_vk_values[] = {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                                 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                                                 VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                 VK_PIPELINE_STAGE_TRANSFER_BIT};
    return s_vk_values[value];
}

static VkAccessFlags util_to_vk_access_flags(ResourceState state) {
    VkAccessFlags ret = 0;
    if (state & RESOURCE_STATE_COPY_SOURCE) {
        ret |= VK_ACCESS_TRANSFER_READ_BIT;
    }
    if (state & RESOURCE_STATE_COPY_DEST) {
        ret |= VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    if (state & RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) {
        ret |= VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (state & RESOURCE_STATE_INDEX_BUFFER) {
        ret |= VK_ACCESS_INDEX_READ_BIT;
    }
    if (state & RESOURCE_STATE_UNORDERED_ACCESS) {
        ret |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    }
    if (state & RESOURCE_STATE_INDIRECT_ARGUMENT) {
        ret |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
    if (state & RESOURCE_STATE_RENDER_TARGET) {
        ret |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    if (state & RESOURCE_STATE_DEPTH_WRITE) {
        ret |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    }
    if (state & RESOURCE_STATE_SHADER_RESOURCE) {
        ret |= VK_ACCESS_SHADER_READ_BIT;
    }
    if (state & RESOURCE_STATE_PRESENT) {
        ret |= VK_ACCESS_MEMORY_READ_BIT;
    }
#ifdef ENABLE_RAYTRACING
    if(state & RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE) {
        ret |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
    }
#endif
    return ret;
}

static VkImageLayout util_to_vk_image_layout(ResourceState usage) {
    if (usage & RESOURCE_STATE_COPY_SOURCE) {
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    if (usage & RESOURCE_STATE_COPY_DEST) {
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }
    if (usage & RESOURCE_STATE_RENDER_TARGET) {
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    if (usage & RESOURCE_STATE_DEPTH_WRITE) {
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    if (usage & RESOURCE_STATE_DEPTH_READ) {
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }
    if (usage & RESOURCE_STATE_UNORDERED_ACCESS) {
        return VK_IMAGE_LAYOUT_GENERAL;
    }
    if (usage & RESOURCE_STATE_SHADER_RESOURCE) {
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    if (usage & RESOURCE_STATE_PRESENT) {
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    if (usage == RESOURCE_STATE_COMMON) {
        return VK_IMAGE_LAYOUT_GENERAL;
    }
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

static VkPipelineStageFlags util_determine_pipeline_stage(VkAccessFlags access_flags, QueueType::Enum queue_type) {
    VkPipelineStageFlags flags = 0;

    switch (queue_type) {
        case QueueType::Graphics: {
            if ((access_flags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0) {
                flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            }
            if ((access_flags &
                 (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0) {
                flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
                flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
#ifdef ENABLE_RAYTRACING
                if(pRenderer->mVulkan.mRaytracingExtension) {
                    flags |= VK_PIPELINE_STAGE_RAY_TRACING_NV;
                }
#endif
            }
            if ((access_flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0) {
                flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            if ((access_flags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0) {
                flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            }
            if ((access_flags &
                 (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0) {
                flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            }
            break;
        }
        case QueueType::Compute: {
            if ((access_flags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
                ((access_flags & (VK_ACCESS_INPUT_ATTACHMENT_READ_BIT))) != 0 ||
                ((access_flags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
                 ((access_flags &
                   (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) !=
                  0))) {
                return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            }
            if ((access_flags &
                 (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0) {
                flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }
            break;
        }
        case QueueType::CopyTransfer: {
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

            // Compatible with both compute and graphisc queues
            if ((access_flags & (VK_ACCESS_INDIRECT_COMMAND_READ_BIT)) != 0) {
                flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
            }
            if ((access_flags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0) {
                flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            if (flags == 0) {
                flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            }

            return flags;
    }
}

static cstring to_stage_defines(VkShaderStageFlagBits value) {
    switch(value) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return "VERTEX";
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return "FRAGMENT";
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return "COMPUTE";
        default:
            return "";
    }
}

static cstring to_compiler_extension(VkShaderStageFlagBits value) {
    switch(value) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return "vert";
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return "frag";
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return "comp";
        default:
            return "";
    }
}

}
