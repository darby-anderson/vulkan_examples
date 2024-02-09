//
// Created by darby on 2/8/2024.
//

#include "game_camera.hpp"

#include "platform.hpp"
#include "numerics.hpp"

#include "cglm/struct/affine.h"
#include "cglm/struct/cam.h"
#include "imgui.h"

namespace puffin {

void GameCamera::init(bool enabled_, f32 rotation_speed_, f32 movement_speed_, f32 movement_delta_) {
    reset();
    enabled = enabled_;

    rotation_speed = rotation_speed_;
    movement_speed = movement_speed_;
    movement_delta = movement_delta_;
}

void GameCamera::reset() {
    target_yaw = 0.0f;
    target_pitch = 0.0f;

    target_movement = camera.position;

    mouse_dragging = false;
    ignore_dragging_frames = 3;
    mouse_sensitivity = 1.0f;
}

// Taken from article:
// http://www.rorydriscoll.com/2016/03/07/frame-rate-independent-damping-using-lerp/
float lerp(float a, float b, float t, float dt) {
    return glm_lerp(a, b, 1.f - pow(1 - t, dt));
}

vec3s lerp3(const vec3s& from, const vec3s& to, f32 t, f32 dt) {
    return vec3s{
        lerp(from.x, to.x, t, dt),
        lerp(from.y, to.y, t, dt),
        lerp(from.z, to.z, t, dt)
    };
}

void GameCamera::update(puffin::InputService* input, u32 window_width, u32 window_height, f32 delta_time) {

    if(!enabled) {
        return;
    }

    camera.update();

    if(input->is_mouse_dragging(MouseButton::MOUSE_BUTTON_RIGHT) && !ImGui::IsAnyItemHovered()) {
        if(ignore_dragging_frames == 0) {
            target_yaw -= (input->mouse_position.x - roundu32(window_width / 2.f)) * mouse_sensitivity * delta_time;
            target_pitch -= (input->mouse_position.y - roundu32(window_height / 2.f)) * mouse_sensitivity * delta_time;
        } else {
            ignore_dragging_frames--;
        }
        mouse_dragging = true;
    } else {
        mouse_dragging = false;
        ignore_dragging_frames = 3;
    }

    vec3s camera_movement {0, 0, 0};
    float camera_movement_delta = movement_delta;

    if ( input->is_key_down( Key::KEY_LEFT_SHIFT ) ) {
        camera_movement_delta *= 10.0f;
    }

    if ( input->is_key_down( Key::KEY_LEFT_ALT ) ) {
        camera_movement_delta *= 100.0f;
    }

    if ( input->is_key_down( Key::KEY_LEFT_CONTROL ) ) {
        camera_movement_delta *= 0.1f;
    }

    if ( input->is_key_down( KEY_LEFT ) || input->is_key_down( KEY_A ) ) {
        camera_movement = glms_vec3_add( camera_movement, glms_vec3_scale( camera.right, -camera_movement_delta ) );
    } else if ( input->is_key_down( KEY_RIGHT ) || input->is_key_down( KEY_D ) ) {
        camera_movement = glms_vec3_add( camera_movement, glms_vec3_scale( camera.right, camera_movement_delta ) );
    }

    if ( input->is_key_down( KEY_PAGE_DOWN ) || input->is_key_down( KEY_E ) ) {
        camera_movement = glms_vec3_add( camera_movement, glms_vec3_scale( camera.up, -camera_movement_delta ) );
    } else if ( input->is_key_down( KEY_PAGE_UP ) || input->is_key_down( KEY_Q ) ) {
        camera_movement = glms_vec3_add( camera_movement, glms_vec3_scale( camera.up, camera_movement_delta ) );
    }

    if ( input->is_key_down( KEY_UP ) || input->is_key_down( KEY_W ) ) {
        camera_movement = glms_vec3_add( camera_movement, glms_vec3_scale( camera.direction, camera_movement_delta ) );
    } else if ( input->is_key_down( KEY_DOWN ) || input->is_key_down( KEY_S ) ) {
        camera_movement = glms_vec3_add( camera_movement, glms_vec3_scale( camera.direction, -camera_movement_delta ) );
    }

    target_movement = glms_vec3_add((vec3s&) target_movement, camera_movement);

    {
        // Update camera rotation
        const f32 tween_rotation_speed = rotation_speed * delta_time;
        camera.rotate((target_pitch - camera.pitch) * tween_rotation_speed,
                      (target_yaw - camera.yaw) * tween_rotation_speed);

        // Update camera position
        const f32 tween_position_speed = movement_speed * delta_time;
        camera.position = lerp3(camera.position, target_movement, 0.9f, tween_position_speed);
    }

}

void GameCamera::apply_jittering(f32 x, f32 y) {
    // Reset camera position
    camera.calculate_projection_matrix();

    mat4s jittering_matrix = glms_translate_make({x, y, 0.0f});
    camera.projection = glms_mat4_mul(jittering_matrix, camera.projection);
    camera.calculate_view_projection();
}


}

