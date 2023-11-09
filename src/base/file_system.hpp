//
// Created by darby on 2/10/2023.
//

#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <iterator>

#include "memory.hpp"

namespace puffin {

using FileHandle = FILE*;

std::vector<uint32_t> loadFileIntoUint32_t(std::string filePath) {

    std::ifstream infile; // {filePath, std::ios_base::binary};
    std::vector<uint32_t> buffer;

    infile.open(filePath, std::ios_base::binary);
    infile.seekg(0, std::ios::end);
    size_t filesize = infile.tellg();
    infile.seekg(0, std::ios::beg);

    buffer.resize(filesize / sizeof(uint32_t)); // should be 4 bytes

    infile.read((char *)buffer.data(), filesize);

    return buffer;
}

bool            file_delete(cstring path);
bool            file_read_binary(cstring filename, Allocator* allocator, size_t* size);

}
