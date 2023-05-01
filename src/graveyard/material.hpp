//
// Created by darby on 2/10/2023.
//

#pragma once

#include <spirv_cross/spirv_cross.hpp>

#include "../base/file_system.hpp"

// Looking at: http://kylehalladay.com/blog/tutorial/2017/11/27/Vulkan-Material-System.html
// And: https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide

struct material {

public:
    void test_load_spv(){

        // load spirv from file
        std::vector<uint32_t> vertSpirv = loadFileIntoUint32_t("../../shaders/vert.spv");
        std::vector<uint32_t> fragSpirv = loadFileIntoUint32_t("../../shaders/frag.spv");

        spirv_cross::Compiler comp {std::move(vertSpirv)};

        std::cout << "compiled comp: " << comp.compile() << std::endl;
        auto active = comp.get_active_interface_variables();
        spirv_cross::ShaderResources resources = comp.get_shader_resources(active);

        std::cout << "resource get! --- " << resources.uniform_buffers[0].name << std::endl;

    }

};


