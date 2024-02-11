//
// Created by darby on 2/11/2024.
//

#include "spriv_parser.hpp"

#include "numerics.hpp"
#include "string.hpp"

#include <string.h>

namespace puffin {
namespace spirv {

static const u32        k_bindless_texture_binding = 10;

struct Member
{
    u32         id_index;
    u32         offset;

    StringView  name;
};

struct Id {
    SpvOp       op;
    u32         set;
    u32         binding;

    // For integers and floats
    u8          width;
    u8          sign;

    // For arrays, vectors and matrices
    u32         type_index;
    u32         count;

    // For variables
    SpvStorageClass     storage_class;

    // For constants
    u32         value;

    // For structs
    StringView  name;
    Array<Member>   members;
};

VkShaderStageFlags parse_execution_model(SpvExecutionModel model)
{
    switch(model)
    {
        case(SpvExecutionModelVertex):
        {
            return VK_SHADER_STAGE_VERTEX_BIT;
        }
        case(SpvExecutionModelGeometry):
        {
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        }
        case(SpvExecutionModelFragment):
        {
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        case(SpvExecutionModelKernel):
        {
            return VK_SHADER_STAGE_COMPUTE_BIT;
        }
    }

    return 0;
}

}
}