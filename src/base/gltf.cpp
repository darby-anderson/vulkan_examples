//
// Created by darby on 12/10/2023.
//

#include "gltf.hpp"

#include "json.hpp"

#include "assert.hpp"
#include "file_system.hpp"

using json = nlohmann::json;

namespace puffin {

static void* allocate_and_zero(Allocator* allocator, size_t size) {
    void* result = allocator->allocate(size, 64);
    memset(result, 0, size);

    return result;
}

static void try_load_string(json& json_data, cstring key, StringBuffer& string_buffer, Allocator* allocator) {
    auto it = json_data.find(key);
    if(it == json_data.end()) {
        return;
    }

    std::string value = json_data.value(key, "");

    string_buffer.init(allocator, value.length() + 1);
    string_buffer.append(value.c_str());
}

static void try_load_int(json& json_data, cstring key, i32& value) {
    auto it = json_data.find(key);

    if(it == json_data.end()) {
        value = glTF::INVALID_INT_VALUE;
        return;
    }

    value = json_data.value(key, 0);
}

static void try_load_float(json& json_data, cstring key, f32& value) {
    auto it = json_data.find(key);

    if(it == json_data.end()) {
        value = glTF::INVALID_FLOAT_VALUE;
        return;
    }

    value = json_data.value(key, 0.0f);
}

static void try_load_bool(json& json_data, cstring key, bool& value) {
    auto it = json_data.find(key);

    if(it == json_data.end()) {
        value = false;
        return;
    }

    value = json_data.value(key, false);
}

static void try_load_type(json& json_data, cstring key, glTF::Accessor::Type& type) {
    std::string value = json_data.value(key, "");

    if(value == "SCALAR") {
        type = glTF::Accessor::Type::Scalar;
    } else if(value == "VEC2") {
        type = glTF::Accessor::Type::Vec2;
    } else if(value == "VEC3") {
        type = glTF::Accessor::Type::Vec3;
    } else if(value == "VEC4") {
        type = glTF::Accessor::Type::Vec4;
    } else if(value == "MAT2") {
        type = glTF::Accessor::Type::Mat2;
    } else if(value == "MAT3") {
        type = glTF::Accessor::Type::Mat3;
    } else if(value == "MAT4") {
        type = glTF::Accessor::Type::Mat4;
    } else {
        PASSERT(false);
    }

}

static void try_load_int_array(json& json_data, cstring key, u32& count, i32** array, Allocator* allocator) {
    auto it = json_data.find(key);

    if(it == json_data.end()) {
        count = 0;
        *array = nullptr;
        return;
    }

    json json_array = json_data.at(key);

    count = json_array.size();

    i32* values = (i32*) allocate_and_zero(allocator, sizeof(i32) * count);

    for(size_t i = 0; i < count; i++) {
        values[i] = json_array.at(i);
    }

    *array = values;
}

static void try_load_float_array(json& json_data, cstring key, u32& count, f32** array, Allocator* allocator) {
    auto it = json_data.find(key);

    if(it == json_data.end()) {
        count = 0;
        *array = nullptr;
        return;
    }

    json json_array = json_data.at(key);

    count = json_array.size();

    f32* values = (f32*) allocate_and_zero(allocator, sizeof(f32) * count);

    for(size_t i = 0; i < count; i++) {
        values[i] = json_array.at(i);
    }

    *array = values;
}

static void load_asset(json& json_data, glTF::Asset& asset, Allocator* allocator) {
    json json_asset = json_data["asset"];

    try_load_string(json_asset, "copyright", asset.copyright, allocator);
    try_load_string(json_asset, "generator", asset.generator, allocator);
    try_load_string(json_asset, "minVersion", asset.minVersion, allocator);
    try_load_string(json_asset, "version", asset.version, allocator);
}

static void load_scene(json& json_data, glTF::Scene& scene, Allocator* allocator) {
    try_load_int_array(json_data, "nodes", scene.nodes_count, &scene.nodes, allocator);
}

static void load_scenes(json& json_data, glTF::glTF& gltf_data, Allocator* allocator) {
    json scenes = json_data["scenes"];

    size_t scene_count = scenes.size();
    gltf_data.scenes = (glTF::Scene*) allocate_and_zero(allocator, sizeof(glTF::Scene) * scene_count);
    gltf_data.scene_count = scene_count;

    for(size_t i = 0; i < scene_count; i++) {
        load_scene(scenes[i], gltf_data.scenes[i], allocator);
    }

}

static void load_buffer(json& json_data, glTF::Buffer& buffer, Allocator* allocator) {
    try_load_string(json_data, "uri", buffer.uri, allocator);
    try_load_int(json_data, "byteLength", buffer.byte_length);
    try_load_string(json_data, "name", buffer.name, allocator);
}










}


