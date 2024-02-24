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
                PASSERT(word_count >= 4);

                SpvExecutionModel model = (SpvExecutionModel)data[word_index + 1];
                stage = parse_execution_model(model);
                PASSERT(stage != 0);

                break;
            }
            case (SpvOpDecorate):
            {
                PASSERT(word_count >= 3);

                u32 id_index = data[word_index + 1];
                PASSERT(id_index < id_bound);

                Id& id = ids[id_index];

                SpvDecoration decoration = (SpvDecoration)data[word_count + 2];
                switch(decoration) {
                    case(SpvDecorationBinding):
                    {
                        id.binding = data[word_index + 3];
                        break;
                    }
                    case(SpvDecorationDescriptorSet):
                    {
                        id.binding = data[word_index + 3];
                        break;
                    }
                }

                break;
            }
            case (SpvOpMemberDecorate):
            {
                PASSERT(word_count >= 4);

                u32 id_index = data[word_index + 1];
                PASSERT(id_index < id_bound);

                Id& id = ids[id_index];

                u32 member_index = data[word_index + 2];

                if(id.members.capacity == 0) {
                    id.members.init(allocator, 64, 64);
                }

                Member& member = id.members[member_index];

                SpvDecoration decoration = (SpvDecoration)data[word_index + 3];
                switch(decoration)
                {
                    case(SpvDecorationOffset):
                    {
                        member.offset = data[word_index + 4];
                        break;
                    }
                }

                break;
            }
            case(SpvOpName):
            {
                PASSERT(word_count >= 3);

                u32 id_index = data[word_index + 1];
                PASSERT(id_index < id_bound);

                Id& id = ids[id_index];

                char* name = (char*)(data + (word_index + 2));
                char* name_view = name_buffer.append_use(name);

                id.name.text = name_view;
                id.name.length = strlen(name_view);

                break;
            }
            case(SpvOpMemberName):
            {
                PASSERT(word_count >= 4);

                u32 id_index = data[word_index + 1];
                PASSERT(id_index < id_bound);

                Id& id = ids[id_index];

                u32 member_index = data[word_index + 2];

                if(id.members.capacity == 0) {
                    id.members.init(allocator, 64, 64);
                }

                Member& member = id.members[member_index];

                char* name = (char*)(data + (word_index + 3));
                char* name_view = name_buffer.append_use(name);

                member.name.text = name_view;
                member.name.length = strlen(name_view);

                break;
            }
            case(SpvOpTypeInt):
            {
                PASSERT(word_count == 4);

                u32 id_index = data[word_index + 1];
                PASSERT(id_index < id_bound);

                Id& id = ids[id_index];
                id.op = op;
                id.width = (u8) data[word_index + 2];
                id.sign = (u8) data[word_index + 3];

                return;
            }
            case ( SpvOpTypeFloat ):
            {
                PASSERT( word_count == 3 );

                u32 id_index = data[ word_index + 1 ];
                PASSERT( id_index < id_bound );

                Id& id= ids[ id_index ];
                id.op = op;
                id.width = ( u8 )data[ word_index + 2 ];

                break;
            }
            case ( SpvOpTypeVector ):
            {
                PASSERT( word_count == 4 );

                u32 id_index = data[ word_index + 1 ];
                PASSERT( id_index < id_bound );

                Id& id= ids[ id_index ];
                id.op = op;
                id.type_index = data[ word_index + 2 ];
                id.count = data[ word_index + 3 ];

                break;
            }
            case ( SpvOpTypeMatrix ):
            {
                PASSERT( word_count == 4 );

                u32 id_index = data[ word_index + 1 ];
                PASSERT( id_index < id_bound );

                Id& id= ids[ id_index ];
                id.op = op;
                id.type_index = data[ word_index + 2 ];
                id.count = data[ word_index + 3 ];

                break;
            }
            case ( SpvOpTypeImage ):
            {
                PASSERT( word_count >= 9 );

                break;
            }
            case ( SpvOpTypeSampler ):
            {
                PASSERT( word_count == 2 );

                u32 id_index = data[ word_index + 1 ];
                PASSERT( id_index < id_bound );

                Id& id= ids[ id_index ];
                id.op = op;

                break;
            }
            case ( SpvOpTypeSampledImage ):
            {
                PASSERT( word_count == 3 );

                u32 id_index = data[ word_index + 1 ];
                PASSERT( id_index < id_bound );

                Id& id= ids[ id_index ];
                id.op = op;

                break;
            }
            case ( SpvOpTypeArray ):
            {
                PASSERT( word_count == 4 );

                u32 id_index = data[ word_index + 1 ];
                PASSERT( id_index < id_bound );

                Id& id= ids[ id_index ];
                id.op = op;
                id.type_index = data[ word_index + 2 ];
                id.count = data[ word_index + 3 ];

                break;
            }

        }

    }

}

}
}