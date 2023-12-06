//
// Created by darby on 12/5/2023.
//

#pragma once

#include <stdint.h>
#include "service.hpp"
#include "platform.hpp"

namespace puffin {

struct GpuDevice;
struct CommandBuffer;
struct TextureHandle;

enum ImGuiStyles {
    Default = 0,
    GreenBlue,
    DarkRed,
    DarkGold
};

struct ImGuiServiceConfiguration {
    GpuDevice*              gpu;
    void*                   window_handle;
};


struct ImGuiService : public puffin::Service {

    PUFFIN_DECLARE_SERVICE(ImGuiService);

    void                    init(void* configuration) override;
    void                    shutdown() override;

    void                    new_frame();
    void                    render(CommandBuffer& commands);

    // removes the texture from the cache and destroys the associated descriptor set
    void                    remove_cached_texture(TextureHandle& texture);
    void                    set_style(ImGuiStyles style);

    GpuDevice*              gpu;

    static constexpr cstring    k_name = "puffin_imgui_service";
};

// Application log
void            imgui_log_init();
void            imgui_log_shutdown();

void            imgui_log_draw();

// FPS Graph
void            imgui_fps_init();
void            imgui_fps_shutdown();
void            imgui_fps_add(f32 dt);
void            imgui_fps_draw();

}
