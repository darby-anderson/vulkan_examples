//
// Created by darby on 2/10/2023.
//

#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <iterator>

std::vector<uint32_t> loadFileIntoUint32_t(std::string filePath) {

    std::ifstream infile {filePath, std::ios_base::binary};
    std::vector<uint32_t> buffer;

    infile.seekg(0, std::ios::end);
    size_t filesize = infile.tellg();
    infile.seekg(0, std::ios::beg);

    buffer.resize(filesize / sizeof(uint32_t)); // should be 4 bytes

    infile.read((char *)buffer.data(), filesize);

    return buffer;

}