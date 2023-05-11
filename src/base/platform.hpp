//
// Created by darby on 2/26/2023.
//
#pragma once

#include <stdint.h>


// Macros
#define PuffinArraySize(array)              ( sizeof(array)/sizeof((array)[0]) )

#define PUFFIN_INLINE                       inline
#define PUFFIN_FINLINE                      always_inline
#define PUFFIN_DEBUG_BREAK                  raise(SIGTRAP)
#define PUFFIN_CONCAT_OPERATOR              x y

#define PUFFIN_STRINGIZE( L )               #L
#define PUFFIN_MAKESTRING( L )              PUFFIN_STRINGIZE(L)
#define PUFFIN_CONCAT(x, y)                 PUFFIN_CONCAT_OPERATOR(x, y)
#define PUFFIN_LINE_STRING                  PUFFIN_MAKESTRING( __LINE__ )
#define PUFFIN_FILELINE(MESSAGE)            __FILE__ "(" PUFFIN_LINE_STRING ") : " MESSAGE

// Unique names
#define PUFFIN_UNIQUE_SUFFIX(PARAM)         PUFFIN_CONCAT(PARAM, __LINE__)

// Native types typedefs
typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

typedef float       f32;
typedef double      f64;

typedef const char* cstring;

static const u64    u64_max = UINT64_MAX;
static const i64    i64_max = INT64_MAX;
static const u32    u32_max = UINT32_MAX;
static const i32    i32_max = INT32_MAX;
static const u16    u16_max = UINT16_MAX;
static const i16    i16_max = INT16_MAX;
static const u8     u8_max = INT8_MAX;
static const i8     i8_max = INT8_MAX;