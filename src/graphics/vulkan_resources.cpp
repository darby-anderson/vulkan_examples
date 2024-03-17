//
// Created by darby on 2/28/2023.
//
#include "vulkan_resources.hpp"

namespace puffin {


// Depth Stencil Creation
DepthStencilCreation& DepthStencilCreation::set_depth(bool write, VkCompareOp comparison_test) {
    depth_write_enable = write;
    depth_comparison = comparison_test;
    // Setting depth to 1 means we want to use the depth test.
    depth_enable = 1;

    return *this;
}

// Blend State
BlendState& BlendState::set_color(VkBlendFactor source_color, VkBlendFactor destination_color, VkBlendOp color_operation) {
    this->source_color = source_color;
    this->destination_color = destination_color;
    this->color_operation = color_operation;
    this->blend_enabled = 1;

    return *this;
}

BlendState& BlendState::set_alpha(VkBlendFactor source_alpha, VkBlendFactor destination_alpha, VkBlendOp alpha_operation) {
    this->source_alpha = source_alpha;
    this->destination_alpha = destination_alpha;
    this->alpha_operation = alpha_operation;
    this->separate_blend = 1;

    return *this;
}

BlendState& BlendState::set_color_write_state(ColorWriteEnabled::Mask value) {
    this->color_write_mask = value;

    return *this;
}

// BlendStateCreation

BlendStateCreation& BlendStateCreation::reset() {
    active_states = 0;

    return *this;
}

BlendState& BlendStateCreation::add_blend_state() {
    return blend_states[active_states++];
}

// BufferCreation
BufferCreation& BufferCreation::reset() {
    size = 0;
    initial_data = nullptr;

    return *this;
}

BufferCreation& BufferCreation::set(VkBufferUsageFlags flags, ResourceUsageType::Enum usage_, u32 size_) {
    type_flags = flags;
    usage = usage_;
    size = size_;

    return *this;
}

BufferCreation& BufferCreation::set_data(void* data_) {
    initial_data = data_;

    return *this;
}

BufferCreation& BufferCreation::set_name(const char* name_) {
    name = name_;

    return *this;
}

// Texture Creation
TextureCreation& TextureCreation::set_size(u16 width_, u16 height_, u16 depth_) {
    width = width_;
    height = height_;
    depth = depth_;

    return *this;
}

TextureCreation& TextureCreation::set_flags(u8 mipmaps_, u8 flags_) {
    mipmaps = mipmaps_;
    flags = flags_;

    return *this;
}

TextureCreation& TextureCreation::set_format_type(VkFormat format_, TextureType::Enum type_) {
    format = format_;
    type = type_;

    return *this;
}

TextureCreation& TextureCreation::set_name(const char* name_) {
    name = name_;

    return *this;
}

TextureCreation& TextureCreation::set_data(void* data_) {
    initial_data = data_;

    return *this;
}

// Sampler Creation
SamplerCreation& SamplerCreation::set_min_mag_mip(VkFilter min, VkFilter mag, VkSamplerMipmapMode mip) {
    min_filter = min;
    mag_filter = mag;
    mip_filter = mip;

    return *this;
}

SamplerCreation& SamplerCreation::set_address_mode_u(VkSamplerAddressMode u) {
    address_mode_u = u;

    return *this;
}

SamplerCreation& SamplerCreation::set_address_mode_uv(VkSamplerAddressMode u, VkSamplerAddressMode v) {
    address_mode_u = u;
    address_mode_v = v;

    return *this;
}

SamplerCreation&
SamplerCreation::set_address_mode_uvw(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w) {
    address_mode_u = u;
    address_mode_v = v;
    address_mode_w = w;

    return *this;
}

SamplerCreation& SamplerCreation::set_name(const char* name) {
    this->name = name;

    return *this;
}

// Shader State Creation
ShaderStateCreation& ShaderStateCreation::reset() {
    stages_count = 0;

    return *this;
}

ShaderStateCreation& ShaderStateCreation::set_name(const char* name) {
    this->name = name;

    return *this;
}

ShaderStateCreation& ShaderStateCreation::add_stage(const char* code, u32 code_size, VkShaderStageFlagBits type) {
    stages[stages_count].code = code;
    stages[stages_count].code_size = code_size;
    stages[stages_count].type = type;
    stages_count++;

    return *this;
}

ShaderStateCreation& ShaderStateCreation::set_spv_input(bool value) {
    spv_input = value;

    return *this;
}

// Descriptor Set Layout Creation
DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::reset() {
    num_bindings = 0;
    set_index = 0;

    return *this;
}

DescriptorSetLayoutCreation&
DescriptorSetLayoutCreation::add_binding(const DescriptorSetLayoutCreation::Binding& binding) {
    bindings[num_bindings++] = binding;

    return *this;
}

DescriptorSetLayoutCreation&
DescriptorSetLayoutCreation::add_binding_at_index(const DescriptorSetLayoutCreation::Binding& binding, int index) {
    bindings[index] = binding;
    num_bindings = (index + 1) > num_bindings ? (index + 1) : num_bindings;

    return *this;
}

DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::set_name(cstring name) {
    this->name = name;

    return *this;
}

DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::set_set_index(u32 index) {
    set_index = index;

    return *this;
}

// Descriptor Set Creation
DescriptorSetCreation& DescriptorSetCreation::reset() {
    num_resources = 0;

    return *this;
}

DescriptorSetCreation& DescriptorSetCreation::set_layout(DescriptorSetLayoutHandle layout) {
    this->layout = layout;

    return *this;
}

DescriptorSetCreation& DescriptorSetCreation::texture(TextureHandle texture, u16 binding) {
    // Set a default sampler
    samplers[num_resources] = k_invalid_sampler;
    bindings[num_resources] = binding;
    resources[num_resources] = texture.index;
    num_resources++;

    return *this;
}

DescriptorSetCreation& DescriptorSetCreation::buffer(BufferHandle buffer, u16 binding) {
    samplers[num_resources] = k_invalid_sampler;
    bindings[num_resources] = binding;
    resources[num_resources] = buffer.index;
    num_resources++;

    return *this;
}

DescriptorSetCreation&
DescriptorSetCreation::texture_sampler(TextureHandle texture, SamplerHandle sampler, u16 binding) {
    // Set a default sampler
    samplers[num_resources] = sampler;
    bindings[num_resources] = binding;
    resources[num_resources] = texture.index;
    num_resources++;

    return *this;
}

DescriptorSetCreation& DescriptorSetCreation::set_name(const char* name) {
    this->name = name;

    return *this;
}

// Vertex Input Creation //////////////////
VertexInputCreation& VertexInputCreation::reset() {
    num_vertex_streams = 0;
    num_vertex_attributes = 0;
    return *this;
}

VertexInputCreation& VertexInputCreation::add_vertex_stream(const VertexStream& stream) {
    vertex_streams[num_vertex_streams++] = stream;
    return *this;
}


VertexInputCreation& VertexInputCreation::add_vertex_attribute(const VertexAttribute& attribute) {
    vertex_attributes[num_vertex_attributes++] = attribute;
    return *this;
}

// Render Pass Output
RenderPassOutput& RenderPassOutput::reset() {
    num_color_formats = 0;
    for (u32 i = 0; i < k_max_image_outputs; i++) {
        color_formats[i] = VK_FORMAT_UNDEFINED;
    }
    depth_stencil_format = VK_FORMAT_UNDEFINED;
    color_operation = RenderPassOperation::DontCare;
    depth_operation = RenderPassOperation::DontCare;
    stencil_operation = RenderPassOperation::DontCare;

    return *this;
}

RenderPassOutput& RenderPassOutput::color(VkFormat format) {
    color_formats[num_color_formats++] = format;
    return *this;
}

RenderPassOutput& RenderPassOutput::depth(VkFormat format) {
    depth_stencil_format = format;
    return *this;
}

RenderPassOutput& RenderPassOutput::set_operations(RenderPassOperation::Enum color, RenderPassOperation::Enum depth,
                                                   RenderPassOperation::Enum stencil) {
    color_operation = color;
    depth_operation = depth;
    stencil_operation = stencil;

    return *this;
}

// Render Pass Creation
RenderPassCreation& RenderPassCreation::reset() {
    num_render_targets = 0;
    depth_stencil_texture = k_invalid_texture;
    resize = 0;
    scale_x = 1.f;
    scale_y = 1.f;
    color_operation = RenderPassOperation::DontCare;
    depth_operation = RenderPassOperation::DontCare;
    stencil_operation = RenderPassOperation::DontCare;

    return *this;
}

RenderPassCreation& RenderPassCreation::add_render_texture(TextureHandle texture) {
    output_textures[num_render_targets++] = texture;

    return *this;
}

RenderPassCreation& RenderPassCreation::set_scaling(f32 scale_x, f32 scale_y, u8 resize) {
    this->scale_x = scale_x;
    this->scale_y = scale_y;
    this->resize = resize;

    return *this;
}

RenderPassCreation& RenderPassCreation::set_depth_stencil_texture(TextureHandle texture) {
    depth_stencil_texture = texture;

    return *this;
}

RenderPassCreation& RenderPassCreation::set_name(const char* name) {
    this->name = name;

    return *this;
}

RenderPassCreation& RenderPassCreation::set_type(RenderPassType::Enum type) {
    this->type = type;

    return *this;
}

RenderPassCreation& RenderPassCreation::set_operations(RenderPassOperation::Enum color, RenderPassOperation::Enum depth,
                                                       RenderPassOperation::Enum stencil) {
    this->color_operation = color;
    this->depth_operation = depth;
    this->stencil_operation = stencil;

    return *this;
}

// Pipeline Creation

PipelineCreation& PipelineCreation::add_descriptor_set_layout(DescriptorSetLayoutHandle handle) {
    descriptor_set_layout[num_active_layouts++] = handle;
    return *this;
}

RenderPassOutput& PipelineCreation::render_pass_output() {
    return render_pass;
}

// Execution Barrier

ExecutionBarrier& ExecutionBarrier::reset() {
    num_image_barriers = 0;
    num_memory_barriers = 0;
    destination_pipeline_stage = PipelineStage::DrawIndirect;

    return *this;
}

ExecutionBarrier& ExecutionBarrier::set(PipelineStage::Enum source, PipelineStage::Enum destination) {
    source_pipeline_stage = source;
    destination_pipeline_stage = destination;

    return *this;
}

ExecutionBarrier& ExecutionBarrier::add_image_barrier(const ImageBarrier& image_barrier) {
    image_barriers[num_image_barriers++] = image_barrier;

    return *this;
}

ExecutionBarrier& ExecutionBarrier::add_memory_barrier(const MemoryBarrier& memory_barrier) {
    memory_barriers[num_memory_barriers++] = memory_barrier;

    return *this;
}



};

