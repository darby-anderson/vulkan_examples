cmake_minimum_required(VERSION 3.20)

add_library(
        tlsf
        "tlsf.c"
        "tlsf.h"
)

target_include_directories(tlsf PUBLIC ./)