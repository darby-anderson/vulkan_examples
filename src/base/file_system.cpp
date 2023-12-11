//
// Created by darby on 2/26/2023.
//

#include "file_system.hpp"
#include "memory.hpp"
#include "assert.hpp"
#include "platform.hpp"
#include "string.hpp"

#if defined (_WIN64)
#include <windows.h>
#else
#define MAX_PATH 65536
#include <syslib.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <string.h>

namespace puffin {

void file_open(cstring filename, cstring mode, FileHandle* file) {
#if defined(_WIN64)
    fopen_s(file, filename, mode);
#else
    *file = fopen(filename, mode);
#endif
}

void file_close(FileHandle file) {
    if(file) {
        fclose(file);
    }
}

size_t file_write(uint8_t* memory, u32 element_size, u32 count, FileHandle file) {
    return fwrite(memory, element_size, count, file);
}

static long file_get_size(FileHandle file) {
    long fileSizeSigned;

    fseek(file, 0, SEEK_END);
    fileSizeSigned = ftell(file);
    fseek(file, 0, SEEK_SET);

    return fileSizeSigned;
}

#if defined(_WIN64)
FileTime file_last_write_time(cstring filename) {
    FILETIME last_write_time = {};

    WIN32_FILE_ATTRIBUTE_DATA data;
    if(GetFileAttributesExA(filename, GetFileExInfoStandard, &data)) {
        last_write_time.dwHighDateTime = data.ftLastWriteTime.dwHighDateTime;
        last_write_time.dwLowDateTime = data.ftLastWriteTime.dwLowDateTime;
    }

    return last_write_time;
}
#endif

u32 file_resolve_to_full_path(cstring path, char* out_full_path, u32 max_size) {
#if defined(_WIN64)
    return GetFullPathNameA(path, max_size, out_full_path, nullptr);
#else
    return readlink(path, out_full_path, max_size);
#endif
}

void file_directory_from_path(char* path) {
    char* last_point = strrchr(path, '.');
    char* last_separator = strrchr(path, '/');
    if(last_separator != nullptr && last_point > last_separator) {
        *(last_separator + 1) = 0;
    } else {
        // Try searching backslash
        last_separator = strrchr(path, '\\');
        if(last_separator != nullptr && last_point > last_separator) {
            *(last_separator + 1) = 0;
        } else {
            PASSERTM(false, "Malformed path! %s", path);
        }
    }
}

void file_name_from_path(char* path) {
    char* last_separator = strrchr(path, '/');
    if(last_separator == nullptr) {
        last_separator = strrchr(path, '\\');
    }

    if(last_separator != nullptr) {
        size_t name_length = strlen(last_separator + 1);

        memcpy(path, last_separator + 1, name_length);
        path[name_length] = 0;
    }
}

char* file_extension_from_path(char* path) {
    char* last_point = strrchr(path, '.');
    return last_point + 1;
}

bool file_exists(cstring path) {
#if defined(_WIN64)
    WIN32_FILE_ATTRIBUTE_DATA unused;
    return GetFileAttributesExA(path, GetFileExInfoStandard, &unused);
#else
    int result = access(path, F_OK);
    return (result == 0);
#endif
}


bool file_delete(cstring path) {
    int result = remove(path);
    return result != 0;
}

bool directory_exists(cstring path) {
    WIN32_FILE_ATTRIBUTE_DATA unused;
    return GetFileAttributesExA(path, GetFileExInfoStandard, &unused);
}

bool directory_create(cstring path) {
    int result = CreateDirectoryA(path, NULL);
    return (result != 0);
}

bool directory_delete(cstring path) {
    int result = RemoveDirectoryA(path);
    return (result != 0);
}

void directory_current(Directory* directory) {
    DWORD written_chars = GetCurrentDirectoryA(k_max_path, directory->path);
    directory->path[written_chars] = 0;
}

void directory_change(cstring path) {
    if(!SetCurrentDirectoryA(path)) {
        p_print("cannot set current directory to %s\n", path);
    }
}

static bool string_ends_with_char(cstring s, char c) {
    cstring last_entry = strrchr(s, c);
    const size_t index = last_entry - s;
    return index == (strlen(s) - 1);
}

void file_open_directory(cstring path, Directory* out_directory) {
    // Open file trying to convert to full path instead of relative.
    // In an error occurs, just copy the name
    if(file_resolve_to_full_path(path, out_directory->path, MAX_PATH) == 0) {
        strcpy(out_directory->path, path);
    }

    // Add '\\' if missing
    if(!string_ends_with_char(path, '\\')) {
        strcat(out_directory->path, "\\");
    }

    if(!string_ends_with_char(out_directory->path, '*')) {
        strcat(out_directory->path, "*");
    }

    out_directory->os_handle = nullptr;

    WIN32_FIND_DATAA find_data;
    HANDLE found_handle;

    if((found_handle = FindFirstFileA(out_directory->path, &find_data)) != INVALID_HANDLE_VALUE) {
        out_directory->os_handle = found_handle;
    } else {
        p_print("Could not open directory %s\n", out_directory->path);
    }
}

void file_close_directory(Directory* directory) {
    if(directory->os_handle) {
        FindClose(directory->os_handle);
    }
}

void file_parent_directory(Directory* directory) {
    Directory new_directory;

    cstring last_directory_separator = strrchr(directory->path, '\\');
    size_t index = last_directory_separator - directory->path;

    if(index > 0) {
        strncpy(new_directory.path, directory->path, index);
        new_directory.path[index] = 0;

        last_directory_separator = strrchr(new_directory.path, '\\');
        size_t second_index = last_directory_separator - new_directory.path;

        if(last_directory_separator) {
            new_directory.path[second_index] = 0;
        } else {
            new_directory.path[index] = 0;
        }

        file_open_directory(new_directory.path, &new_directory);
    }

    if(new_directory.os_handle) {
        *directory = new_directory;
    }
}

void file_sub_directory(Directory* directory, cstring sub_directory_name) {

    // Remove the last '*' from the path. It will be re-added by the file_open
    if(string_ends_with_char(directory->path, '*')) {
        directory->path[strlen(directory->path) - 1] = 0;
    }

    strcat(directory->path, sub_directory_name);
    file_open_directory(directory->path, directory);

}

void file_find_files_in_path(cstring file_pattern, StringArray& files) {

    files.clear();

    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    if((hFind = FindFirstFileA(file_pattern, &find_data)) != INVALID_HANDLE_VALUE) {
        do {
            files.intern(find_data.cFileName);
        } while(FindNextFileA(hFind, &find_data) != 0);
        FindClose(hFind);
    } else {
        p_print("Cannot find file %s\n", file_pattern);
    }
}

void file_find_files_in_path(cstring extension, cstring search_pattern, StringArray& files, StringArray& directories) {

    files.clear();
    directories.clear();

    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    if((hFind = FindFirstFileA(search_pattern, &find_data)) != INVALID_HANDLE_VALUE) {
        do {
            if(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                directories.intern(find_data.cFileName);
            } else {
                // If file name contains the extension, add it
                if(strstr(find_data.cFileName, extension)) {
                    files.intern(find_data.cFileName);
                }
            }
        } while(FindNextFileA(hFind, &find_data) != 0);
        FindClose(hFind);
    } else {
        p_print("Cannot find directory %s\n", search_pattern);
    }
}

void environment_variable_get(cstring name, char* output, u32 output_size) {
    ExpandEnvironmentStringsA(name, output, output_size);
}

char* file_read_binary(cstring filename, Allocator* allocator, size_t* size) {
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

char* file_read_text(cstring filename, Allocator* allocator, size_t* size) {
    char* text = 0;

    FILE* file = fopen(filename, "r");

    if(file) {
        // TODO: Use filesize or read result
        size_t file_size = file_get_size(file);

        text = (char*)puffin_alloc(file_size + 1, allocator);
        size_t bytes_read = fread(text, file_size, 1, file);

        text[bytes_read] = 0;

        if(size) {
            *size = file_size;
        }

        fclose(file);
    }

    return text;
}

FileReadResult file_read_binary(cstring filename, Allocator* allocator) {

    FileReadResult result {nullptr, 0};

    FILE* file = fopen(filename, "rb");

    if(file) {
        // TODO: Use filesize or read result
        size_t file_size = file_get_size(file);

        result.data = (char*)puffin_alloc(file_size + 1, allocator);
        fread(result.data, file_size, 1, file);

        result.size = file_size;

        fclose(file);
    }

    return result;
}

FileReadResult file_read_text(cstring filename, Allocator* allocator) {

    FileReadResult result {nullptr, 0};

    FILE* file = fopen(filename, "r");

    if(file) {
        size_t file_size = file_get_size(file);
        result.data = (char*)puffin_alloc(file_size + 1, allocator);

        size_t bytes_read = fread(result.data, 1, file_size, file);
        result.data[bytes_read] = 0;

        result.size = file_size;

        fclose(file);
    }

    return result;
}

void file_write_binary(cstring filename, void* memory, size_t size) {
    FILE* file = fopen(filename, "wb");
    fwrite(memory, size, 1, file);
    fclose(file);
}

// Scoped File
ScopedFile::ScopedFile(cstring filename, cstring mode) {
    file_open(filename, mode, &file);
}

ScopedFile::~ScopedFile() {
    file_close(file);
}

}