//
// Created by darby on 2/27/2023.
//

#pragma once

#include "platform.hpp"

namespace ResourceUsageType {
    enum Enum {
        Immutable, Dynamic, Stream, Count
    };

    enum Mask {
        Immutable_mask = 1 << 0, Dynamic_mask = 1 << 1, Stream_mask = 1 << 2, Count_mask = 1 << 3
    };

    static const char* s_value_names[] = {
            "Immutable", "Dynamic", "Stream", "Count"
    };

    static const char* ToString(Enum e) {
        return ((u32)e < Enum::Count ? s_value_names[(int)e] : "unsupported");
    }
}

namespace TextureType {

    enum Enum {
        Texture1D, Texture2D, Texture3D, Texture_1D_Array, Texture_2D_Array, Texture_Cube_Array, Count
    };

    enum Mask {
        Texture1D_mask = 1 << 0, Texture2D_mask = 1 << 2, Texture3D_mask = 1 << 3, Texture_1D_Array_mask = 1 << 4,
        Texture_2D_Array_mask = 1 << 5, Texture_Cube_Array_mask = 1 << 6, Count_mask = 1 << 7
    };

    static const char* s_value_names[] = {
            "Texture1D", "Texture2D", "Texture3D", "Texture_1D_Array", "Texture_2D_Array",
            "Texture_Cube_Array", "Count"
    };

    static const char* ToString(Enum e) {
        return ((u32)e < Enum::Count ? s_value_names[(int)e] : "unsupported");
    }
};

namespace RenderPassType {
    enum Enum {
        Geometry, Swapchain, Compute
    };
}

namespace RenderPassOperation {
    enum Enum {
        DontCare, Load, Clear, Count
    };
}

namespace VertexComponentFormat {
    enum Enum {
        Float, Float2, Float3, Float4, Mat4, Byte, Byte4N, UByte, UByte4N, Short2, Short2N, Short4, Short4N, Uint, Uint2, Uint4, Count
    };

    static const char* s_value_names[] = {
            "Float", "Float2", "Float3", "Float4", "Mat4", "Byte", "Byte4N", "UByte", "UByte4N", "Short2", "Short2N", "Short4", "Short4N", "Uint", "Uint2", "Uint4", "Count"
    };

    static const char* ToString( Enum e ) {
        return ((u32)e < Enum::Count ? s_value_names[(int)e] : "unsupported" );
    }
}


namespace VertexInputRate {
    enum Enum {
        PerVertex, PerInstance, Count
    };

    enum Mask {
        PerVertex_mask = 1 << 0, PerInstance_mask = 1 << 1, Count_mask = 1 << 2
    };

    static const char* s_value_names[] = {
            "PerVertex", "PerInstance", "Count"
    };

    static const char* ToString( Enum e ) {
        return ((u32)e < Enum::Count ? s_value_names[(int)e] : "unsupported" );
    }
} // namespace VertexInputRate

namespace PipelineStage {

    enum Enum {
        DrawIndirect = 0, VertexInput = 1, VertexShader = 2, FragmentShader = 3, RenderTarget = 4, ComputeShader = 5, Transfer = 6
    };

    enum Mask {
        DrawIndirect_mask = 1 << 0, VertexInput_mask = 1 << 1, VertexShader_mask = 1 << 2, FragmentShader_mask = 1 << 3, RenderTarget_mask = 1 << 4, ComputeShader_mask = 1 << 5, Transfer_mask = 1 << 6
    };

}