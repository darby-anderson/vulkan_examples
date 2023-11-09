//
// Created by darby on 2/26/2023.
//

#include "file_system.hpp"
#include "platform.hpp"

namespace puffin {

static long file_get_size(FileHandle file) {
    long fileSizeSigned;

    fseek(file, 0, SEEK_END);
    fileSizeSigned = ftell(file);
    fseek(file, 0, SEEK_SET);

    return fileSizeSigned;
}

bool file_read_binary(cstring filename, Allocator* allocator, size_t* size) {
    char* out_data = 0;

    FILE* file = fopen(filename, "rb");

    if(file) {
        // TODO: Use filesize or read result
        size_t file_size = file_get_size(file);

        out_data = (char*)puffin_alloc(file_size + 1, allocator);
        fread(out_data, file_size, 1, file);
        out_data[file_size] = 0;

        if(size) {
            *size = file_size;
        }

        fclose(file);
    }

    return out_data;
}

bool file_delete(cstring path) {
    int result = remove(path);
    return result != 0;
}

}