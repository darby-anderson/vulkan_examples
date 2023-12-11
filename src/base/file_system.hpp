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

struct Allocator;
struct StringArray;

#if defined(_WIN64)
typedef struct __FILETIME {
    unsigned long dwLowDateTime;
    unsigned long dwHighDateTime;
} FILETIME, * PFILETIME, * LPFILETIME;

using FileTime = __FILETIME;
#endif

using FileHandle = FILE*;

static const u32            k_max_path = 512;

struct Directory {
    char                    path[k_max_path];

#if defined(_WIN64)
    void*                   os_handle;
#endif
}; // Directory

struct FileReadResult {
    char*                   data;
    size_t                  size;
};

// read files into memory from allocator
// user must free this memory
char*                       file_read_binary(cstring filename, Allocator* allocator, size_t* size);
char*                       file_read_text(cstring filename, Allocator* allocator, size_t* size);

FileReadResult              file_read_binary(cstring filename, Allocator* allocator);
FileReadResult              file_read_text(cstring filename, Allocator* allocator);

void                        file_write_binary(cstring filename, void* memory, size_t size);

bool                        file_exists(cstring path);
void                        file_open(cstring path, cstring mode, FileHandle* file);
void                        file_close(FileHandle file);
size_t                      file_write(uint8_t* memory, u32 element_size, u32 count, FileHandle file);
bool                        file_delete(cstring path);

#if defined(_WIN64)
FileTime                    file_last_write_time(cstring filename);
#endif

// Get the full path
u32                         file_resolve_to_full_path(cstring path, char* out_full_path, u32 max_size);

// Inplace path methods
void                        file_directory_from_path(char* path);
void                        file_name_from_path(char* path);
char*                       file_extension_from_path(char* path);

bool                        directory_exists(cstring path);
bool                        directory_create(cstring path);
bool                        directory_delete(cstring path);

void                        directory_current(Directory* directory);
void                        directory_change(cstring path);

void                        file_open_directory(cstring path, Directory* out_directory);
void                        file_close_directory(Directory* directory);
void                        file_parent_directory(Directory* directory);
void                        file_sub_directory(Directory* directory, cstring sub_directory_name);

void                        file_find_files_in_path(cstring file_pattern, StringArray& files);
void                        file_find_files_in_path(cstring extension, cstring search_pattern,
                                                    StringArray& files, StringArray& directories);

void                        environment_variable_get(cstring name, char* output, u32 output_size);

struct ScopedFile {
    ScopedFile(cstring filename, cstring mode);
    ~ScopedFile();

    FileHandle              file;
};



//
//std::vector<uint32_t> loadFileIntoUint32_t(std::string filePath) {
//
//    std::ifstream infile; // {filePath, std::ios_base::binary};
//    std::vector<uint32_t> buffer;
//
//    infile.open(filePath, std::ios_base::binary);
//    infile.seekg(0, std::ios::end);
//    size_t filesize = infile.tellg();
//    infile.seekg(0, std::ios::beg);
//
//    buffer.resize(filesize / sizeof(uint32_t)); // should be 4 bytes
//
//    infile.read((char *)buffer.data(), filesize);
//
//    return buffer;
//}
//
//bool            file_exists(cstring path);
//bool            file_delete(cstring path);
//bool            file_read_binary(cstring filename, Allocator* allocator, size_t* size);

}
