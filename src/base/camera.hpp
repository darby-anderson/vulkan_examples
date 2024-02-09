//
// Created by darby on 2/6/2024.
//

#pragma once

#include "platform.hpp"
#include "cglm/struct/mat4.h"

#include ""

namespace puffin {

struct Camera {

    void            init_perspective(f32 near_plane, f32 far_plane, f32 fov_y, f32 aspect_ratio);
    void            init_orthographic(f32 near_plane, f32 far_plane, f32 viewport_width, f32 viewport_height, f32 zoom);

    void            reset();

    void            set_viewport_size(f32 width, f32 height);
    void            set_zoom(f32 zoom);
    void            set_aspect_ratio(f32 aspect_ratio);
    void            set_fov_y(f32 fov_y);

    void            update();
    void            rotate(f32 delta_pitch, f32 delta_yaw);

    void            calculate_projection_matrix();
    void            calculate_view_projection();

    // project/un-project
    vec3s           unproject(const vec3s& screen_coordinates);

    // Un-project by inverting the y of the screen coord
    vec3s           unproject_inverted_y(const vec3s& screen_coordinates);

    void            get_projection_ortho_2d(mat4& out_matrix);

    static void     yaw_pitch_from_direction(const vec3s& direction, f32& yaw, f32& pitch);


    mat4s           view;
    mat4s           projection;
    mat4s           view_projection;

    vec3s           position;
    vec3s           right;
    vec3s           direction;
    vec3s           up;

    f32             yaw;
    f32             pitch;

    f32             near_plane;
    f32             far_plane;

    f32             field_of_view_y;
    f32             aspect_ratio;

    f32             zoom;
    f32             viewport_width;
    f32             viewport_height;

    bool            perspective;
    bool            update_projection;

};

}