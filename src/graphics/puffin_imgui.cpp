//
// Created by darby on 12/5/2023.
//

#include "puffin_imgui.hpp"

#include "hash_map.hpp"
#include "memory.hpp"

#include "gpu_enum.hpp"
#include "gpu_device.hpp"
#include "command_buffer.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include <stdio.h>
#include <iostream>

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
        "layout( location = 0 ) in vec2 Position;\n"
        "layout( location = 1 ) in vec2 UV;\n"
        "layout( location = 2 ) in uvec4 Color;\n"
        "layout( location = 0 ) out vec2 Frag_UV;\n"
        "layout( location = 1 ) out vec4 Frag_Color;\n"
        "layout( std140, binding = 0 ) uniform LocalConstants { mat4 ProjMtx; };\n"
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
        "layout( std140, binding = 0 ) uniform LocalConstants { mat4 ProjMtx; };\n"
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
        "#extension GL_EXT_nonuniform_qualifier : enable\n"
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
        "#extension GL_EXT_nonuniform_qualifier : enable\n"
        "layout (location = 0) in vec2 Frag_UV;\n"
        "layout (location = 1) in vec4 Frag_Color;\n"
        "layout (location = 2) flat in uint texture_id;\n"
        "layout (location = 0) out vec4 Out_Color;\n"
        "#extension GL_EXT_nonuniform_qualifier : enable\n"
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

void ImGuiService::init(void* configuration) {
    ImGuiServiceConfiguration* imgui_config = (ImGuiServiceConfiguration*) configuration;
    gpu = imgui_config->gpu;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Setup platform/renderer bindings
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow*) imgui_config->window_handle, true);

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "Puffin_ImGui";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    using namespace puffin;

    // Load font texture atlas ///////////////
    unsigned char* pixels;
    int width, height;

    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

//    for(int w = 0; w < width; w++) {
//        for(int h = 0; h < height; h++) {
//            std::cout << (u32) pixels[(h * w) + w];
//        }
//        std::cout << std::endl;
//    }

    TextureCreation texture_creation;
    texture_creation.set_format_type(VK_FORMAT_R8G8B8A8_UNORM, TextureType::Texture2D)
            .set_data(pixels).set_size(width, height, 1).set_flags(1, 0).set_name("ImGui_Font");
    g_font_texture = gpu->create_texture(texture_creation);

    // Store the identifier
    io.Fonts->TexID = (ImTextureID)&g_font_texture;

    // Manual code. Used to remove dependency
    ShaderStateCreation shader_creation{};

    if(gpu->bindless_supported) {
        shader_creation.set_name("ImGui")
            .add_stage(g_vertex_shader_code_bindless, (uint32_t) strlen(g_vertex_shader_code_bindless), VK_SHADER_STAGE_VERTEX_BIT)
            .add_stage(g_fragment_shader_code_bindless, (uint32_t) strlen(g_fragment_shader_code_bindless), VK_SHADER_STAGE_FRAGMENT_BIT);
    } else {
        shader_creation.set_name("ImGui")
            .add_stage(g_vertex_shader_code, (uint32_t) strlen(g_vertex_shader_code), VK_SHADER_STAGE_VERTEX_BIT)
            .add_stage(g_fragment_shader_code, (uint32_t) strlen(g_fragment_shader_code), VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    PipelineCreation pipeline_creation{};
    pipeline_creation.name = "Pipeline_ImGui";
    pipeline_creation.shaders = shader_creation;

    pipeline_creation.blend_state.add_blend_state().set_color(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD);

    pipeline_creation.vertex_input.add_vertex_attribute({0, 0, 0, VertexComponentFormat::Float2})
        .add_vertex_attribute({1, 0, 8, VertexComponentFormat::Float2})
        .add_vertex_attribute({2, 0, 16, VertexComponentFormat::UByte4N});

    pipeline_creation.vertex_input.add_vertex_stream({0, 20, VertexInputRate::PerVertex});
    pipeline_creation.render_pass = gpu->get_swapchain_output();

    DescriptorSetLayoutCreation descriptor_set_layout_creation{};
    if(gpu->bindless_supported) {
        descriptor_set_layout_creation.add_binding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 1, "LocalConstants"})
                .add_binding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10, 1, "Texture"})
                .set_name("RLL_ImGui");
    } else {
        descriptor_set_layout_creation.add_binding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 1, "LocalConstants"}).set_name("RLL_ImGui");
        descriptor_set_layout_creation.add_binding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1, "LocalConstants"}).set_name("RLL_ImGui");
    }

    g_descriptor_set_layout = gpu->create_descriptor_set_layout(descriptor_set_layout_creation);

    pipeline_creation.add_descriptor_set_layout(g_descriptor_set_layout);

    g_imgui_pipeline = gpu->create_pipeline(pipeline_creation);

    // Create constant buffer
    BufferCreation cb_creation;
    cb_creation.set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceUsageType::Dynamic, 64).set_name("CB_ImGui");
    g_ui_cb = gpu->create_buffer(cb_creation);

    // Create descriptor set
    DescriptorSetCreation ds_creation{};
    if(gpu->bindless_supported) {
        ds_creation.set_layout(pipeline_creation.descriptor_set_layout[0])
            .buffer(g_ui_cb, 0)
            .texture(g_font_texture, 1)
            .set_name("RL_ImGui");
    } else {
        ds_creation.set_layout(pipeline_creation.descriptor_set_layout[0])
            .buffer(g_ui_cb, 0)
            .set_name("RL_ImGui");
    }
    g_ui_descriptor_set = gpu->create_descriptor_set(ds_creation);

    // Add descriptor set to the map
    // old map
    g_texture_to_descriptor_set.init(&MemoryService::instance()->system_allocator, 4);
    g_texture_to_descriptor_set.insert(g_font_texture.index, g_ui_descriptor_set.index);

    // Create vert and index buffers ///////
    BufferCreation vb_creation;
    vb_creation.set(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, ResourceUsageType::Dynamic, g_vb_size).set_name("VB_ImGui");
    g_vb = gpu->create_buffer(vb_creation);

    BufferCreation ib_creation;
    ib_creation.set(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, ResourceUsageType::Dynamic, g_ib_size).set_name("IB_ImGui");
    g_ib = gpu->create_buffer(ib_creation);
}

void ImGuiService::shutdown() {
    FlatHashMapIterator it = g_texture_to_descriptor_set.iterator_begin();

    while(it.is_valid()) {
        puffin::ResourceHandle handle = g_texture_to_descriptor_set.get(it);
        gpu->destroy_descriptor_set({handle});

        g_texture_to_descriptor_set.iterator_advance(it);
    }

    g_texture_to_descriptor_set.shutdown();

    gpu->destroy_buffer(g_vb);
    gpu->destroy_buffer(g_ib);
    gpu->destroy_buffer(g_ui_cb);
    gpu->destroy_descriptor_set_layout(g_descriptor_set_layout);

    gpu->destroy_pipeline(g_imgui_pipeline);
    gpu->destroy_texture(g_font_texture);

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiService:: new_frame() {
    // ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiService::render(puffin::CommandBuffer& commands) {

    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();

    // Avoid rendering when minimized
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if(fb_width <= 0 || fb_height <= 0) {
        return;
    }

    // vulkan backend has a different origin than opengl
    bool clip_origin_lower_left = false;
#if defined(GL_CLIP_ORIGIN) && !defined(__APPLE__)
    GLenum last_clip_origin = 0; glGetIntegerv( GL_CLIP_ORIGIN, (GLint*)&last_clip_origin ); // Support for GL 4.5's glClipControl(GL_UPPER_LEFT)
    if ( last_clip_origin == GL_UPPER_LEFT )
        clip_origin_lower_left = false;
#endif

    size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

    if(vertex_size >= g_vb_size || index_size >= g_ib_size) {
        p_print("ImGui backend error, vertex/index overflow\n");
        return;
    }

    if(vertex_size == 0 && index_size == 0) {
        return;
    }

    using namespace puffin;

    // Upload data to gpu
    ImDrawVert* vtx_dst = NULL;
    ImDrawIdx* idx_dst = NULL;

    MapBufferParameters map_parameters_vb = { g_vb, 0, (u32)vertex_size};
    vtx_dst = (ImDrawVert*) gpu->map_buffer(map_parameters_vb);

    if(vtx_dst) {
        for(int n = 0; n < draw_data->CmdListsCount; n++) {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            vtx_dst += cmd_list->VtxBuffer.Size;
        }

        gpu->unmap_buffer(map_parameters_vb);
    }

    MapBufferParameters map_parameters_ib = { g_ib, 0, (u32)index_size};
    idx_dst = (ImDrawIdx*) gpu->map_buffer(map_parameters_ib);

    if(idx_dst) {
        for(int n = 0; n < draw_data->CmdListsCount; n++) {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            idx_dst += cmd_list->IdxBuffer.Size;
        }

        gpu->unmap_buffer(map_parameters_ib);
    }

    commands.push_marker("ImGui");

    commands.bind_pass(gpu->get_swapchain_pass());
    commands.bind_pipeline(g_imgui_pipeline);
    commands.bind_vertex_buffer(g_vb, 0, 0);
    commands.bind_index_buffer(g_ib, 0, VK_INDEX_TYPE_UINT16);

    const Viewport viewport = {0, 0, (u16)fb_width, (u16)fb_height, 0.0f, 1.0f };
    commands.set_viewport(&viewport);

    // Setup viewport, orthographic projection matrix
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    const float ortho_projection[4][4] =
    {
            { 2.0f / ( R - L ),   0.0f,         0.0f,   0.0f },
            { 0.0f,         2.0f / ( T - B ),   0.0f,   0.0f },
            { 0.0f,         0.0f,        -1.0f,   0.0f },
            { ( R + L ) / ( L - R ),  ( T + B ) / ( B - T ),  0.0f,   1.0f },
    };

    MapBufferParameters cb_map = {g_ui_cb, 0, 0};
    float* cb_data = (float*)gpu->map_buffer(cb_map);
    if(cb_data) {
        memcpy(cb_data, &ortho_projection[0][0], 64);
        gpu->unmap_buffer(cb_map);
    }

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;
    ImVec2 clip_scale = draw_data->FramebufferScale;

    // render command lists
    int counts = draw_data->CmdListsCount;

    TextureHandle last_texture = g_font_texture;
    DescriptorSetHandle last_descriptor_set = {g_texture_to_descriptor_set.get(last_texture.index)};
    commands.bind_descriptor_set(&last_descriptor_set, 1, nullptr, 0);

    uint32_t vtx_buffer_offset = 0, index_buffer_offset = 0;
    for(int n = 0; n < counts; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];

        for(int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmd_list, pcmd);
            } else {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec4 clip_rect;
                clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
                clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
                clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
                clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

                if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
                    // apply scissors/clipping rect
                    if (clip_origin_lower_left) {
                        Rect2DInt scissor_rect = {(int16_t) clip_rect.x, (int16_t) (fb_height - clip_rect.w),
                                                  (uint16_t) (clip_rect.z - clip_rect.x),
                                                  (uint16_t) (clip_rect.w - clip_rect.y)};
                        commands.set_scissors(&scissor_rect);
                    } else {
                        Rect2DInt scissor_rect = {(int16_t) clip_rect.x, (int16_t) clip_rect.y,
                                                  (uint16_t) (clip_rect.z - clip_rect.x),
                                                  (uint16_t) (clip_rect.w - clip_rect.y)};
                        commands.set_scissors(&scissor_rect);
                    }

                    TextureHandle new_texture = *(TextureHandle*) (pcmd->TextureId);
                    if (!gpu->bindless_supported) {
                        if (new_texture.index != last_texture.index && new_texture.index != k_invalid_texture.index) {
                        last_texture = new_texture;
                        FlatHashMapIterator it = g_texture_to_descriptor_set.find(last_texture.index);

                        if (it.is_invalid()) {
                            // Create new descriptor set
                            DescriptorSetCreation ds_creation{};

                            ds_creation.set_layout(g_descriptor_set_layout).buffer(g_ui_cb, 0)
                                    .texture(last_texture, 1).set_name("RL_Dynamic_ImGui");
                            last_descriptor_set = gpu->create_descriptor_set(ds_creation);

                            g_texture_to_descriptor_set.insert(new_texture.index, last_descriptor_set.index);
                        } else {
                            last_descriptor_set.index = g_texture_to_descriptor_set.get(it);
                        }
                        commands.bind_descriptor_set(&last_descriptor_set, 1, nullptr, 0);
                    }
                }
                commands.draw_indexed(TopologyType::Triangle, pcmd->ElemCount, 1,
                                      index_buffer_offset + pcmd->IdxOffset, vtx_buffer_offset + pcmd->VtxOffset,
                                          new_texture.index);
                }

            }
        }
        index_buffer_offset += cmd_list->IdxBuffer.Size;
        vtx_buffer_offset += cmd_list->VtxBuffer.Size;
    }
    commands.pop_marker();
}

static void set_style_dark_gold();
static void set_style_green_blue();
static void set_style_dark_red();

void ImGuiService::set_style( ImGuiStyles style ) {

    switch ( style ) {
        case GreenBlue:
        {
            set_style_green_blue();
            break;
        }

        case DarkRed:
        {
            set_style_dark_red();
            break;
        }

        case DarkGold:
        {
            set_style_dark_gold();
            break;
        }

        default:
        case Default:
        {
            ImGui::StyleColorsDark();
            break;
        }
    }
}

void ImGuiService::remove_cached_texture(puffin::TextureHandle& texture) {
    FlatHashMapIterator it = g_texture_to_descriptor_set.find(texture.index);
    if(it.is_valid()) {
        // Destroy descriptor set
        puffin::DescriptorSetHandle descriptor_set {g_texture_to_descriptor_set.get(it)};
        gpu->destroy_descriptor_set(descriptor_set);

        // Remove from cache
        g_texture_to_descriptor_set.remove(texture.index);
    }
}


void set_style_dark_red() {
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ ImGuiCol_Text ] = ImVec4( 0.75f, 0.75f, 0.75f, 1.00f );
    colors[ ImGuiCol_TextDisabled ] = ImVec4( 0.35f, 0.35f, 0.35f, 1.00f );
    colors[ ImGuiCol_WindowBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.94f );
    colors[ ImGuiCol_ChildBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_PopupBg ] = ImVec4( 0.08f, 0.08f, 0.08f, 0.94f );
    colors[ ImGuiCol_Border ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.50f );
    colors[ ImGuiCol_BorderShadow ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_FrameBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.54f );
    colors[ ImGuiCol_FrameBgHovered ] = ImVec4( 0.37f, 0.14f, 0.14f, 0.67f );
    colors[ ImGuiCol_FrameBgActive ] = ImVec4( 0.39f, 0.20f, 0.20f, 0.67f );
    colors[ ImGuiCol_TitleBg ] = ImVec4( 0.04f, 0.04f, 0.04f, 1.00f );
    colors[ ImGuiCol_TitleBgActive ] = ImVec4( 0.48f, 0.16f, 0.16f, 1.00f );
    colors[ ImGuiCol_TitleBgCollapsed ] = ImVec4( 0.48f, 0.16f, 0.16f, 1.00f );
    colors[ ImGuiCol_MenuBarBg ] = ImVec4( 0.14f, 0.14f, 0.14f, 1.00f );
    colors[ ImGuiCol_ScrollbarBg ] = ImVec4( 0.02f, 0.02f, 0.02f, 0.53f );
    colors[ ImGuiCol_ScrollbarGrab ] = ImVec4( 0.31f, 0.31f, 0.31f, 1.00f );
    colors[ ImGuiCol_ScrollbarGrabHovered ] = ImVec4( 0.41f, 0.41f, 0.41f, 1.00f );
    colors[ ImGuiCol_ScrollbarGrabActive ] = ImVec4( 0.51f, 0.51f, 0.51f, 1.00f );
    colors[ ImGuiCol_CheckMark ] = ImVec4( 0.56f, 0.10f, 0.10f, 1.00f );
    colors[ ImGuiCol_SliderGrab ] = ImVec4( 1.00f, 0.19f, 0.19f, 0.40f );
    colors[ ImGuiCol_SliderGrabActive ] = ImVec4( 0.89f, 0.00f, 0.19f, 1.00f );
    colors[ ImGuiCol_Button ] = ImVec4( 1.00f, 0.19f, 0.19f, 0.40f );
    colors[ ImGuiCol_ButtonHovered ] = ImVec4( 0.80f, 0.17f, 0.00f, 1.00f );
    colors[ ImGuiCol_ButtonActive ] = ImVec4( 0.89f, 0.00f, 0.19f, 1.00f );
    colors[ ImGuiCol_Header ] = ImVec4( 0.33f, 0.35f, 0.36f, 0.53f );
    colors[ ImGuiCol_HeaderHovered ] = ImVec4( 0.76f, 0.28f, 0.44f, 0.67f );
    colors[ ImGuiCol_HeaderActive ] = ImVec4( 0.47f, 0.47f, 0.47f, 0.67f );
    colors[ ImGuiCol_Separator ] = ImVec4( 0.32f, 0.32f, 0.32f, 1.00f );
    colors[ ImGuiCol_SeparatorHovered ] = ImVec4( 0.32f, 0.32f, 0.32f, 1.00f );
    colors[ ImGuiCol_SeparatorActive ] = ImVec4( 0.32f, 0.32f, 0.32f, 1.00f );
    colors[ ImGuiCol_ResizeGrip ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.85f );
    colors[ ImGuiCol_ResizeGripHovered ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.60f );
    colors[ ImGuiCol_ResizeGripActive ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.90f );
    colors[ ImGuiCol_Tab ] = ImVec4( 0.07f, 0.07f, 0.07f, 0.51f );
    colors[ ImGuiCol_TabHovered ] = ImVec4( 0.86f, 0.23f, 0.43f, 0.67f );
    colors[ ImGuiCol_TabActive ] = ImVec4( 0.19f, 0.19f, 0.19f, 0.57f );
    colors[ ImGuiCol_TabUnfocused ] = ImVec4( 0.05f, 0.05f, 0.05f, 0.90f );
    colors[ ImGuiCol_TabUnfocusedActive ] = ImVec4( 0.13f, 0.13f, 0.13f, 0.74f );
#if defined(IMGUI_HAS_DOCK)
    colors[ ImGuiCol_DockingPreview ] = ImVec4( 0.47f, 0.47f, 0.47f, 0.47f );
colors[ ImGuiCol_DockingEmptyBg ] = ImVec4( 0.20f, 0.20f, 0.20f, 1.00f );
#endif // IMGUI_HAS_DOCK
    colors[ ImGuiCol_PlotLines ] = ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
    colors[ ImGuiCol_PlotLinesHovered ] = ImVec4( 1.00f, 0.43f, 0.35f, 1.00f );
    colors[ ImGuiCol_PlotHistogram ] = ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
    colors[ ImGuiCol_PlotHistogramHovered ] = ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
#if defined(IMGUI_HAS_TABLE)
    colors[ ImGuiCol_TableHeaderBg ] = ImVec4( 0.19f, 0.19f, 0.20f, 1.00f );
    colors[ ImGuiCol_TableBorderStrong ] = ImVec4( 0.31f, 0.31f, 0.35f, 1.00f );
    colors[ ImGuiCol_TableBorderLight ] = ImVec4( 0.23f, 0.23f, 0.25f, 1.00f );
    colors[ ImGuiCol_TableRowBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_TableRowBgAlt ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.07f );
#endif // IMGUI_HAS_TABLE
    colors[ ImGuiCol_TextSelectedBg ] = ImVec4( 0.26f, 0.59f, 0.98f, 0.35f );
    colors[ ImGuiCol_DragDropTarget ] = ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
    colors[ ImGuiCol_NavHighlight ] = ImVec4( 0.26f, 0.59f, 0.98f, 1.00f );
    colors[ ImGuiCol_NavWindowingHighlight ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
    colors[ ImGuiCol_NavWindowingDimBg ] = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
    colors[ ImGuiCol_ModalWindowDimBg ] = ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );
}


void set_style_green_blue() {
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ ImGuiCol_Text ] = ImVec4( 1.00f, 1.00f, 1.00f, 1.00f );
    colors[ ImGuiCol_TextDisabled ] = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
    colors[ ImGuiCol_WindowBg ] = ImVec4( 0.06f, 0.06f, 0.06f, 0.94f );
    colors[ ImGuiCol_ChildBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_PopupBg ] = ImVec4( 0.08f, 0.08f, 0.08f, 0.94f );
    colors[ ImGuiCol_Border ] = ImVec4( 0.43f, 0.43f, 0.50f, 0.50f );
    colors[ ImGuiCol_BorderShadow ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_FrameBg ] = ImVec4( 0.44f, 0.44f, 0.44f, 0.60f );
    colors[ ImGuiCol_FrameBgHovered ] = ImVec4( 0.57f, 0.57f, 0.57f, 0.70f );
    colors[ ImGuiCol_FrameBgActive ] = ImVec4( 0.76f, 0.76f, 0.76f, 0.80f );
    colors[ ImGuiCol_TitleBg ] = ImVec4( 0.04f, 0.04f, 0.04f, 1.00f );
    colors[ ImGuiCol_TitleBgActive ] = ImVec4( 0.16f, 0.16f, 0.16f, 1.00f );
    colors[ ImGuiCol_TitleBgCollapsed ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.60f );
    colors[ ImGuiCol_MenuBarBg ] = ImVec4( 0.14f, 0.14f, 0.14f, 1.00f );
    colors[ ImGuiCol_ScrollbarBg ] = ImVec4( 0.02f, 0.02f, 0.02f, 0.53f );
    colors[ ImGuiCol_ScrollbarGrab ] = ImVec4( 0.31f, 0.31f, 0.31f, 1.00f );
    colors[ ImGuiCol_ScrollbarGrabHovered ] = ImVec4( 0.41f, 0.41f, 0.41f, 1.00f );
    colors[ ImGuiCol_ScrollbarGrabActive ] = ImVec4( 0.51f, 0.51f, 0.51f, 1.00f );
    colors[ ImGuiCol_CheckMark ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.80f );
    colors[ ImGuiCol_SliderGrab ] = ImVec4( 0.13f, 0.75f, 0.75f, 0.80f );
    colors[ ImGuiCol_SliderGrabActive ] = ImVec4( 0.13f, 0.75f, 1.00f, 0.80f );
    colors[ ImGuiCol_Button ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.40f );
    colors[ ImGuiCol_ButtonHovered ] = ImVec4( 0.13f, 0.75f, 0.75f, 0.60f );
    colors[ ImGuiCol_ButtonActive ] = ImVec4( 0.13f, 0.75f, 1.00f, 0.80f );
    colors[ ImGuiCol_Header ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.40f );
    colors[ ImGuiCol_HeaderHovered ] = ImVec4( 0.13f, 0.75f, 0.75f, 0.60f );
    colors[ ImGuiCol_HeaderActive ] = ImVec4( 0.13f, 0.75f, 1.00f, 0.80f );
    colors[ ImGuiCol_Separator ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.40f );
    colors[ ImGuiCol_SeparatorHovered ] = ImVec4( 0.13f, 0.75f, 0.75f, 0.60f );
    colors[ ImGuiCol_SeparatorActive ] = ImVec4( 0.13f, 0.75f, 1.00f, 0.80f );
    colors[ ImGuiCol_ResizeGrip ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.40f );
    colors[ ImGuiCol_ResizeGripHovered ] = ImVec4( 0.13f, 0.75f, 0.75f, 0.60f );
    colors[ ImGuiCol_ResizeGripActive ] = ImVec4( 0.13f, 0.75f, 1.00f, 0.80f );
    colors[ ImGuiCol_Tab ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.80f );
    colors[ ImGuiCol_TabHovered ] = ImVec4( 0.13f, 0.75f, 0.75f, 0.80f );
    colors[ ImGuiCol_TabActive ] = ImVec4( 0.13f, 0.75f, 1.00f, 0.80f );
    colors[ ImGuiCol_TabUnfocused ] = ImVec4( 0.18f, 0.18f, 0.18f, 1.00f );
    colors[ ImGuiCol_TabUnfocusedActive ] = ImVec4( 0.36f, 0.36f, 0.36f, 0.54f );
#if defined(IMGUI_HAS_DOCK)
    colors[ ImGuiCol_DockingPreview ] = ImVec4( 0.13f, 0.75f, 0.55f, 0.80f );
colors[ ImGuiCol_DockingEmptyBg ] = ImVec4( 0.13f, 0.13f, 0.13f, 0.80f );
#endif // IMGUI_HAS_DOCK
    colors[ ImGuiCol_PlotLines ] = ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
    colors[ ImGuiCol_PlotLinesHovered ] = ImVec4( 1.00f, 0.43f, 0.35f, 1.00f );
    colors[ ImGuiCol_PlotHistogram ] = ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
    colors[ ImGuiCol_PlotHistogramHovered ] = ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
#if defined (IMGUI_HAS_TABLE)
    colors[ ImGuiCol_TableHeaderBg ] = ImVec4( 0.19f, 0.19f, 0.20f, 1.00f );
    colors[ ImGuiCol_TableBorderStrong ] = ImVec4( 0.31f, 0.31f, 0.35f, 1.00f );
    colors[ ImGuiCol_TableBorderLight ] = ImVec4( 0.23f, 0.23f, 0.25f, 1.00f );
    colors[ ImGuiCol_TableRowBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_TableRowBgAlt ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.07f );
#endif // IMGUI_HAS_TABLE
    colors[ ImGuiCol_TextSelectedBg ] = ImVec4( 0.26f, 0.59f, 0.98f, 0.35f );
    colors[ ImGuiCol_DragDropTarget ] = ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
    colors[ ImGuiCol_NavHighlight ] = ImVec4( 0.26f, 0.59f, 0.98f, 1.00f );
    colors[ ImGuiCol_NavWindowingHighlight ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
    colors[ ImGuiCol_NavWindowingDimBg ] = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
    colors[ ImGuiCol_ModalWindowDimBg ] = ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );
}

static void set_style_dark_gold() {
    ImGuiStyle* style = &ImGui::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ ImGuiCol_Text ] = ImVec4( 0.92f, 0.92f, 0.92f, 1.00f );
    colors[ ImGuiCol_TextDisabled ] = ImVec4( 0.44f, 0.44f, 0.44f, 1.00f );
    colors[ ImGuiCol_WindowBg ] = ImVec4( 0.06f, 0.06f, 0.06f, 1.00f );
    colors[ ImGuiCol_ChildBg ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_PopupBg ] = ImVec4( 0.08f, 0.08f, 0.08f, 0.94f );
    colors[ ImGuiCol_Border ] = ImVec4( 0.51f, 0.36f, 0.15f, 1.00f );
    colors[ ImGuiCol_BorderShadow ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ ImGuiCol_FrameBg ] = ImVec4( 0.11f, 0.11f, 0.11f, 1.00f );
    colors[ ImGuiCol_FrameBgHovered ] = ImVec4( 0.51f, 0.36f, 0.15f, 1.00f );
    colors[ ImGuiCol_FrameBgActive ] = ImVec4( 0.78f, 0.55f, 0.21f, 1.00f );
    colors[ ImGuiCol_TitleBg ] = ImVec4( 0.51f, 0.36f, 0.15f, 1.00f );
    colors[ ImGuiCol_TitleBgActive ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_TitleBgCollapsed ] = ImVec4( 0.00f, 0.00f, 0.00f, 0.51f );
    colors[ ImGuiCol_MenuBarBg ] = ImVec4( 0.11f, 0.11f, 0.11f, 1.00f );
    colors[ ImGuiCol_ScrollbarBg ] = ImVec4( 0.06f, 0.06f, 0.06f, 0.53f );
    colors[ ImGuiCol_ScrollbarGrab ] = ImVec4( 0.21f, 0.21f, 0.21f, 1.00f );
    colors[ ImGuiCol_ScrollbarGrabHovered ] = ImVec4( 0.47f, 0.47f, 0.47f, 1.00f );
    colors[ ImGuiCol_ScrollbarGrabActive ] = ImVec4( 0.81f, 0.83f, 0.81f, 1.00f );
    colors[ ImGuiCol_CheckMark ] = ImVec4( 0.78f, 0.55f, 0.21f, 1.00f );
    colors[ ImGuiCol_SliderGrab ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_SliderGrabActive ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_Button ] = ImVec4( 0.51f, 0.36f, 0.15f, 1.00f );
    colors[ ImGuiCol_ButtonHovered ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_ButtonActive ] = ImVec4( 0.78f, 0.55f, 0.21f, 1.00f );
    colors[ ImGuiCol_Header ] = ImVec4( 0.51f, 0.36f, 0.15f, 1.00f );
    colors[ ImGuiCol_HeaderHovered ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_HeaderActive ] = ImVec4( 0.93f, 0.65f, 0.14f, 1.00f );
    colors[ ImGuiCol_Separator ] = ImVec4( 0.21f, 0.21f, 0.21f, 1.00f );
    colors[ ImGuiCol_SeparatorHovered ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_SeparatorActive ] = ImVec4( 0.78f, 0.55f, 0.21f, 1.00f );
    colors[ ImGuiCol_ResizeGrip ] = ImVec4( 0.21f, 0.21f, 0.21f, 1.00f );
    colors[ ImGuiCol_ResizeGripHovered ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_ResizeGripActive ] = ImVec4( 0.78f, 0.55f, 0.21f, 1.00f );
    colors[ ImGuiCol_Tab ] = ImVec4( 0.51f, 0.36f, 0.15f, 1.00f );
    colors[ ImGuiCol_TabHovered ] = ImVec4( 0.91f, 0.64f, 0.13f, 1.00f );
    colors[ ImGuiCol_TabActive ] = ImVec4( 0.78f, 0.55f, 0.21f, 1.00f );
    colors[ ImGuiCol_TabUnfocused ] = ImVec4( 0.07f, 0.10f, 0.15f, 0.97f );
    colors[ ImGuiCol_TabUnfocusedActive ] = ImVec4( 0.14f, 0.26f, 0.42f, 1.00f );
    colors[ ImGuiCol_PlotLines ] = ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
    colors[ ImGuiCol_PlotLinesHovered ] = ImVec4( 1.00f, 0.43f, 0.35f, 1.00f );
    colors[ ImGuiCol_PlotHistogram ] = ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
    colors[ ImGuiCol_PlotHistogramHovered ] = ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
    colors[ ImGuiCol_TextSelectedBg ] = ImVec4( 0.26f, 0.59f, 0.98f, 0.35f );
    colors[ ImGuiCol_DragDropTarget ] = ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
    colors[ ImGuiCol_NavHighlight ] = ImVec4( 0.26f, 0.59f, 0.98f, 1.00f );
    colors[ ImGuiCol_NavWindowingHighlight ] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
    colors[ ImGuiCol_NavWindowingDimBg ] = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
    colors[ ImGuiCol_ModalWindowDimBg ] = ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );

    style->FramePadding = ImVec2( 4, 2 );
    style->ItemSpacing = ImVec2( 10, 2 );
    style->IndentSpacing = 12;
    style->ScrollbarSize = 10;

    style->WindowRounding = 4;
    style->FrameRounding = 4;
    style->PopupRounding = 4;
    style->ScrollbarRounding = 6;
    style->GrabRounding = 4;
    style->TabRounding = 4;

    style->WindowTitleAlign = ImVec2( 1.0f, 0.5f );
    style->WindowMenuButtonPosition = ImGuiDir_Right;

    style->DisplaySafeAreaPadding = ImVec2( 4, 4 );
}



// Usage:
//  static ExampleAppLog my_log;
//  my_log.AddLog("Hello %d world\n", 123);
//  my_log.Draw("title");
struct ExampleAppLog {
    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets;        // Index to lines offset. We maintain this with AddLog() calls, allowing us to have a random access on lines
    bool                AutoScroll;     // Keep scrolling if already at the bottom

    ExampleAppLog() {
        AutoScroll = true;
        Clear();
    }

    void    Clear() {
        Buf.clear();
        LineOffsets.clear();
        LineOffsets.push_back( 0 );
    }

    void    AddLog( const char* fmt, ... ) IM_FMTARGS( 2 ) {
        int old_size = Buf.size();
        va_list args;
                va_start( args, fmt );
        Buf.appendfv( fmt, args );
                va_end( args );
        for ( int new_size = Buf.size(); old_size < new_size; old_size++ )
            if ( Buf[ old_size ] == '\n' )
                LineOffsets.push_back( old_size + 1 );
    }

    void    Draw( const char* title, bool* p_open = NULL ) {
        if ( !ImGui::Begin( title, p_open ) ) {
            ImGui::End();
            return;
        }

        // Options menu
        if ( ImGui::BeginPopup( "Options" ) ) {
            ImGui::Checkbox( "Auto-scroll", &AutoScroll );
            ImGui::EndPopup();
        }

        // Main window
        if ( ImGui::Button( "Options" ) )
            ImGui::OpenPopup( "Options" );
        ImGui::SameLine();
        bool clear = ImGui::Button( "Clear" );
        ImGui::SameLine();
        bool copy = ImGui::Button( "Copy" );
        ImGui::SameLine();
        Filter.Draw( "Filter", -100.0f );

        ImGui::Separator();
        ImGui::BeginChild( "scrolling", ImVec2( 0, 0 ), false, ImGuiWindowFlags_HorizontalScrollbar );

        if ( clear )
            Clear();
        if ( copy )
            ImGui::LogToClipboard();

        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );
        const char* buf = Buf.begin();
        const char* buf_end = Buf.end();
        if ( Filter.IsActive() ) {
            // In this example we don't use the clipper when Filter is enabled.
            // This is because we don't have a random access on the result on our filter.
            // A real application processing logs with ten of thousands of entries may want to store the result of search/filter.
            // especially if the filtering function is not trivial (e.g. reg-exp).
            for ( int line_no = 0; line_no < LineOffsets.Size; line_no++ ) {
                const char* line_start = buf + LineOffsets[ line_no ];
                const char* line_end = ( line_no + 1 < LineOffsets.Size ) ? ( buf + LineOffsets[ line_no + 1 ] - 1 ) : buf_end;
                if ( Filter.PassFilter( line_start, line_end ) )
                    ImGui::TextUnformatted( line_start, line_end );
            }
        } else {
            // The simplest and easy way to display the entire buffer:
            //   ImGui::TextUnformatted(buf_begin, buf_end);
            // And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward to skip non-visible lines.
            // Here we instead demonstrate using the clipper to only process lines that are within the visible area.
            // If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them on your side is recommended.
            // Using ImGuiListClipper requires A) random access into your data, and B) items all being the  same height,
            // both of which we can handle since we an array pointing to the beginning of each line of text.
            // When using the filter (in the block of code above) we don't have random access into the data to display anymore, which is why we don't use the clipper.
            // Storing or skimming through the search result would make it possible (and would be recommended if you want to search through tens of thousands of entries)
            ImGuiListClipper clipper;
            clipper.Begin( LineOffsets.Size );
            while ( clipper.Step() ) {
                for ( int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++ ) {
                    const char* line_start = buf + LineOffsets[ line_no ];
                    const char* line_end = ( line_no + 1 < LineOffsets.Size ) ? ( buf + LineOffsets[ line_no + 1 ] - 1 ) : buf_end;
                    ImGui::TextUnformatted( line_start, line_end );
                }
            }
            clipper.End();
        }
        ImGui::PopStyleVar();

        if ( AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() )
            ImGui::SetScrollHereY( 1.0f );

        ImGui::EndChild();
        ImGui::End();
    }
}; // struct ExampleAppLog

static ExampleAppLog        s_imgui_log;
static bool                 s_imgui_log_open = true;

static void imgui_print( const char* text ) {
    s_imgui_log.AddLog( "%s", text );
}

void imgui_log_init() {

    LogService::instance()->set_callback( &imgui_print );
}

void imgui_log_shutdown() {

    LogService::instance()->set_callback( nullptr );
}

void imgui_log_draw() {
    s_imgui_log.Draw( "Log", &s_imgui_log_open );
}


// Plot with ringbuffer

// https://github.com/leiradel/ImGuiAl
template<typename T, size_t L>
class Sparkline {
public:
    Sparkline() {
        setLimits( 0, 1 );
        clear();
    }

    void setLimits( T const min, T const max ) {
        _min = static_cast< float >( min );
        _max = static_cast< float >( max );
    }

    void add( T const value ) {
        _offset = ( _offset + 1 ) % L;
        _values[ _offset ] = value;
    }

    void clear() {
        memset( _values, 0, L * sizeof( T ) );
        _offset = L - 1;
    }

    void draw( char const* const label = "", ImVec2 const size = ImVec2() ) const {
        char overlay[ 32 ];
        print( overlay, sizeof( overlay ), _values[ _offset ] );

        ImGui::PlotLines( label, getValue, const_cast< Sparkline* >( this ), L, 0, overlay, _min, _max, size );
    }

protected:
    float _min, _max;
    T _values[ L ];
    size_t _offset;

    static float getValue( void* const data, int const idx ) {
        Sparkline const* const self = static_cast< Sparkline* >( data );
        size_t const index = ( idx + self->_offset + 1 ) % L;
        return static_cast< float >( self->_values[ index ] );
    }

    static void print( char* const buffer, size_t const bufferLen, int const value ) {
        snprintf( buffer, bufferLen, "%d", value );
    }

    static void print( char* const buffer, size_t const bufferLen, double const value ) {
        snprintf( buffer, bufferLen, "%f", value );
    }
};

static Sparkline<f32, 100> s_fps_line;

void imgui_fps_init() {
    s_fps_line.clear();
    s_fps_line.setLimits( 0.0f, 33.f );
}

void imgui_fps_shutdown() {
}

void imgui_fps_add( f32 dt ) {
    s_fps_line.add( dt );
}

void imgui_fps_draw() {
    s_fps_line.draw( "FPS", { 0,100 } );
}

}
