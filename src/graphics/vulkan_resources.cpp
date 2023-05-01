//
// Created by darby on 2/28/2023.
//
#include "vulkan_resources.hpp"

// BufferCreation
BufferCreation& BufferCreation::reset() {
    size = 0;
    initialData = nullptr;

    return *this;
}

BufferCreation& BufferCreation::set(VkBufferUsageFlags flags, ResourceUsageType::Enum usage_, u32 size_) {
    type_flags = flags;
    usage = usage_;
    size = size_;

    return *this;
}

BufferCreation& BufferCreation::set_data(void* data_) {
    initialData = data_;

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

TextureCreation& TextureCreation::set_name(const char *name_) {
    name = name_;

    return *this;
}

TextureCreation& TextureCreation::set_data(void *data_) {
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

SamplerCreation& SamplerCreation::set_address_mode_uvw(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w) {
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

DescriptorSetLayoutCreation& DescriptorSetLayoutCreation::add_binding(const DescriptorSetLayoutCreation::Binding& binding) {
    bindings[num_bindings++] = binding;

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

DescriptorSetCreation& DescriptorSetCreation::texture_sampler(TextureHandle texture, SamplerHandle sampler, u16 binding) {
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


VertexInputCreation& VertexInputCreation::reset() {
    num_vertex_streams = 0;
    num_vertex_attributes = 0;
    return *this;
}

VertexInputCreation& VertexInputCreation::add_vertex_stream(const VertexStream& stream) {
    vertex_streams[num_vertex_streams++] = stream;
    return *this;
}


VertexInputCreation& VertexInputCreation::add_vertex_stream(const VertexAttribute& attribute) {
    vertex_attributes[num_vertex_attributes++] = attribute;
    return *this;
}

// Render Pass Output
RenderPassOutput& RenderPassOutput::reset() {
    num_color_formats = 0;
    for(u32 i = 0; i < k_max_image_outputs; i++) {
        color_formats[i] = VK_FORMAT_UNDEFINED;
    }

    return *this;
}

// Render Pass Creation
RenderPassCreation& RenderPassCreation::reset() {
    num_color_formats = 0;

    return <#initializer#>;
}



