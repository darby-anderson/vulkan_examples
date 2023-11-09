//
// Created by darby on 8/9/2023.
//
#pragma once

#include "process.hpp"
#include "assert.hpp"
#include "log.hpp"

#include "memory.hpp"
#include "string.hpp"

#include <stdio.h>

#if defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace puffin {

    static const u32    k_process_log_buffer = 256;
    char                s_process_log_buffer[k_process_log_buffer];
    static char         k_process_output_buffer[1025];

#if defined(_WIN64)

    void win32_get_error(char* buffer, u32 size) {
        DWORD error_code = GetLastError();

        char* error_string;
        if(!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL,
                           error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error_string, 0, NULL)) {
            return;
        }

        sprintf_s(buffer, size, "%s", error_string);

        LocalFree(error_string);
    }

    bool process_execute(cstring working_dir, cstring process_fullpath, cstring arguments, cstring search_error_string) {
        HANDLE handle_stdin_pipe_read = NULL;
        HANDLE handle_stdin_pipe_write = NULL;
        HANDLE handle_stdout_pipe_read = NULL;
        HANDLE handle_std_pipe_write = NULL;

        SECURITY_ATTRIBUTES security_attributes = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

        BOOL ok = CreatePipe(&handle_stdin_pipe_read, &handle_stdin_pipe_write, &security_attributes, 0);
        if(ok == FALSE) {
            return false;
        }
        ok = CreatePipe(&handle_stdout_pipe_read, &handle_std_pipe_write, &security_attributes, 0);
        if(ok == FALSE) {
            return false;
        }

        // Create startup informations with std redirection
        STARTUPINFOA startup_info = {};
        startup_info.cb = sizeof(startup_info);
        startup_info.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        startup_info.hStdInput = handle_stdin_pipe_read;
        startup_info.hStdError = handle_std_pipe_write;
        startup_info.hStdOutput = handle_std_pipe_write;
        startup_info.wShowWindow = SW_SHOW;

        bool execution_success = false;

        // Execute the process
        PROCESS_INFORMATION process_info = {};
        BOOL inherit_handles = TRUE;

        if(CreateProcessA(process_fullpath, (char*)arguments, 0, 0, inherit_handles, 0, 0, working_dir,
                          &startup_info, &process_info)) {
            CloseHandle(process_info.hThread);
            CloseHandle(process_info.hProcess);

            execution_success = true;
        } else {
            win32_get_error(&s_process_log_buffer[0], k_process_log_buffer);

            p_print("Execute process error. \n Exe: \"%s\" - Args: \"%s\"\n", process_fullpath, arguments, working_dir);
            p_print("Message: %s\n", s_process_log_buffer);
        }

        CloseHandle(handle_stdin_pipe_read);
        CloseHandle(handle_stdin_pipe_write);

        // Output
        DWORD bytes_read;
        ok = ReadFile(handle_stdout_pipe_read, k_process_output_buffer, 1024, &bytes_read, nullptr);

        // consume all outputs
        while(ok == TRUE) {
            k_process_output_buffer[bytes_read] = 0;
            p_print("%s", k_process_output_buffer);

            ok = ReadFile(handle_stdout_pipe_read, k_process_output_buffer, 1024, &bytes_read, nullptr);
        }

        if(strlen(search_error_string) > 0 && strstr(k_process_output_buffer, search_error_string)) {
            execution_success = false;
        }

        p_print("\n");

        // Close handles
        CloseHandle(handle_stdout_pipe_read);
        CloseHandle(handle_stdin_pipe_write);

        DWORD process_exit_code = 0;
        GetExitCodeProcess(process_info.hProcess, &process_exit_code);

        return execution_success;
    }

    cstring process_get_output() {
        return k_process_output_buffer;
    }

}

#endif