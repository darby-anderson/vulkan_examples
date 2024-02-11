//
// Created by darby on 2/11/2024.
//

#pragma once

#include "array.hpp"
#include "vulkan_resources.hpp"

#if defined(_MSC_VER)
#include "spirv-headers/spirv.h"
#else
#include "spriv_cross/spirv.h"
#endif

#include "vulkan/vulkan.h"


namespace puffin {

struct StringBuffer;

namespace spirv {

static const u32 MAX_SET_COUNT = 32;

struct ParseResult {
    u32                             set_count;
    DescriptorSetLayoutCreation     sets[MAX_SET_COUNT];
};

void        parse_binary(const u32* data, size_t data_size, StringBuffer& name_buffer, ParseResult* parse_result);

}
}
