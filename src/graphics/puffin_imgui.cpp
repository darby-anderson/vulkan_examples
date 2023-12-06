//
// Created by darby on 12/5/2023.
//

#include "puffin_imgui.hpp"

#include "hash_map.hpp"
#include "memory.hpp"


#include "gpu_device.hpp"
#include "command_buffer.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"

#include <stdio.h>

namespace puffin {

// Graphics data
static puffin::TextureHandle g_font_texture;
static puffin::PipelineHandle g_imgui_pipeline;
static puffin::BufferHandle g_vb, g_ib;
static puffin::BufferHandle g_ui_cb;
static puffin::DescriptorSetLayoutHandle g_descriptor_set_layout;
static puffin::DescriptorSetHandle g_ui_descriptor_set; // for font

static uint32_t g_vb_size = 665536, g_ib_size = 665536;

puffin::FlatHashMap<puffin::ResourceHandle, puffin::ResourceHandle> g_texture_to_descriptor_set;

static const char* g_vertex_shader_code = {
        "#version 450\n"
        "layout( location = 0 ) in vec3 Position;\n"
        "layout( location = 1 ) in vec2 UV;\n"
        "layout( location = 2 ) in uvec4 Color;\n"
        "layout( location = 0 ) out vec2 Frag_UV;\n"
        "layout( location = 1 ) out vec4 Frag_Color;\n"
        "layout( std140, binding = 0 ) out uniform LocalConstants { mat4 ProjMtx; };\n"
        "void main()\n"
        "{\n"
        "   Frag_UV = UV;\n"
        "   Frag_Color = Color / 255.0f;\n"
        "   gl_Position = ProjMtx * vec4( Position.xy, 0, 1 );\n"
        "}\n"
};

static const char* g_vertex_shader_code_bindless = {
        "#version 450\n"
        "layout( location = 0 ) in vec3 Position;\n"
        "layout( location = 1 ) in vec2 UV;\n"
        "layout( location = 2 ) in uvec4 Color;\n"
        "layout( location = 0 ) out vec2 Frag_UV;\n"
        "layout( location = 1 ) out vec4 Frag_Color;\n"
        "layout( location = 2 ) flat out uint texture_id;\n"
        "layout( std140, binding = 0 ) out uniform LocalConstants { mat4 ProjMtx; };\n"
        "void main()\n"
        "{\n"
        "   Frag_UV = UV;\n"
        "   Frag_Color = Color / 255.0f;\n"
        "   texture_id = gl_InstanceIndex;\n"
        "   gl_Position = ProjMtx * vec4( Position.xy, 0, 1 );\n"
        "}\n"
};

static const char* g_fragment_shader_code = {
        "#version 450\n"
        "#extension GL_EXT_nonuniform_qualifer : enable\n"
        "layout (location = 0) in vec2 Frag_UV;\n"
        "layout (location = 1) in vec4 Frag_Color;\n"
        "layout (location = 0) out vec4 Out_Color;\n"
        "layout (binding = 1) uniform sampler2D Texture;\n"
        "void main()\n"
        "{\n"
        "   Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
        "}\n"
};

static const char* g_fragment_shader_code_bindless = {
        "#version 450\n"
        "#extension GL_EXT_nonuniform_qualifer : enable\n"
        "layout (location = 0) in vec2 Frag_UV;\n"
        "layout (location = 1) in vec4 Frag_Color;\n"
        "layout (location = 2) flat in uint texture_id;\n"
        "layout (location = 0) out vec4 Out_Color;\n"
        "#extension GL_EXT_nonuniform_qualifer : enable\n"
        "layout (set = 1, binding = 10) uniform sampler2D textures[];\n"
        "void main()\n"
        "{\n"
        "   Out_Color = Frag_Color * texture(textures[nonuniformEXT(texture_id)], Frag_UV.st);\n"
        "}\n"
};

static ImGuiService s_imgui_service;

ImGuiService* ImGuiService::instance() {
    return &s_imgui_service;
}


}