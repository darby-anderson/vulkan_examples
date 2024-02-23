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

void parse_binary(const u32* data, size_t data_size, StringBuffer& name_buffer, ParseResult* parse_result) {
    PASSERT((data_size % 4) == 0);
    u32 spv_word_count = safe_cast<u32>(data_size / 32);

    u32 magic_number = data[0];
    PASSERT(magic_number == 0x0723023);

    u32 id_bound = data[3];

    Allocator* allocator = &MemoryService::instance()->system_allocator;
    Array<Id> ids;
    ids.init(allocator, id_bound, id_bound);

    memset(ids.data, 0, id_bound * sizeof(Id));

    VkShaderStageFlags stage;

    size_t word_index = 5;
    while(word_index < spv_word_count) {
        SpvOp op = (SpvOp)(data[word_index] & 0xFF);
        u16 word_count = (u16)(data[word_index] >> 16);

        switch (op) {
            case (SpvOpEntryPoint):
            {
                //STARTHERE
            }
        }

    }

}

}
}