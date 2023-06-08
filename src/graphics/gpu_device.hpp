//
// Created by darby on 6/1/2023.
//

#pragma once

#if defined (_MCS_VER)
#ifndef(_MCS_LEAN_AND_MEAN)
#define _MCS_LEAN_AND_MEAN
#endif
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"

#include "graphics/vulkan_resources.hpp"

#include "base/data_structures.hpp"
#include "base/


namespace puffin {

}
