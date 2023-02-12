//
// Created by darby on 2/6/2023.
//

#pragma once

#include <stdexcept>
#include <unordered_map>

#include <tiny_obj_loader.h>

#include "vertex.hpp"

namespace vub {

struct pc_model {
private:
public:
    std::vector<vub::pc_vertex> vertices;
    std::vector<uint32_t> indices;

    void loadObjFile(std::string filePath);
};


struct pctc_model {
private:
public:
    std::vector<vub::pctc_vertex> vertices;
    std::vector<uint32_t> indices;

    void loadObjFile(std::string filePath);
};

} // vub
