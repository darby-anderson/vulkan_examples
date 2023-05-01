//
// Created by darby on 2/6/2023.
//

#pragma once

#include <vulkan/vulkan.h>
#include "glm/glm.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/hash.hpp"

#include <iostream>
#include <array>


namespace puffin {

/*
 * Position, Color vertex
 * Struct that mimics the data being passed into the vertex shader
 */
struct pc_vertex {
    glm::vec3 pos;
    glm::vec3 color;

    /*
     * binding - index of the binding in the array of bindings
     * stride - number of bytes from one entry to the next
     * inputRate - either per-vertex or per-instance
     */
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(pc_vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }
    /*
     * VkVertexInputAttributeDescription - describes how to extract a vertex attribute from a chunk of vertex data
     * originating from a binding description
     */
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions {};
        // Position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(pc_vertex, pos);
        // Color
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(pc_vertex, color);

        return attributeDescriptions;
    }

    // for use in unordered map
    bool operator==(const pc_vertex& other) const {
        return pos == other.pos && color == other.color;
    }
};



/*
* Position, Color and TexCoord vertex
* Struct that mimics the data being passed into the vertex shader.
*/
struct pctc_vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    /*
    * binding - index of the binding in the array of bindings
    * stride - number of bytes from one entry to the next
    * inputRate - either per-vertex or per-instance
    */
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(pctc_vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    /*
    * VkVertexInputAttributeDescription - describes how to extract a vertex attribute from a chunk of vertex data origination from a bind description
    */
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions {};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(pctc_vertex, pos);

        std::cout << "sizeof(Vertex): " << sizeof(pctc_vertex) << " | offsetof(Vertex, pos): " << offsetof(pctc_vertex, pos) << " | offsetof(Vertex, color): " << offsetof(pctc_vertex, color) << " | sizeof(color): " << sizeof(color) << std::endl;;

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(pctc_vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(pctc_vertex, texCoord);

        return attributeDescriptions;
    }

    // for use in unordered map
    bool operator==(const pctc_vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

};

/*
* Creates a hash function for pc_vertex, for use with unordered map
*/
namespace std {
    template<> struct hash<puffin::pc_vertex> {
        size_t operator()(puffin::pc_vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                     (hash<glm::vec3>()(vertex.color) << 1)) >> 1);
        }
    };
}


/*
* Creates a hash function for pctc_vertex, for use with unordered map
*/
namespace std {
    template<> struct hash<puffin::pctc_vertex> {
        size_t operator()(puffin::pctc_vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                     (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}
