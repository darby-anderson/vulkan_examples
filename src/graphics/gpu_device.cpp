//
// Created by darby on 6/1/2023.
//
#include "gpu_device.hpp"
#include "vulkan_resources.hpp"
#include "command_buffer.hpp"
#include "spirv_parser.hpp"

#include "memory.hpp"
#include "hash_map.hpp"
#include "process.hpp"
#include "file_system.hpp"


template<class T>
constexpr const T& puffin_min(const T& a, const T& b) {
    return (a < b) ? a : b;
}

template<class T>
constexpr const T& puffin_max(const T& a, const T& b) {
    return (a < b) ? b : a;
}

#define VMA_MAX puffin_max
#define VMA_MIN puffin_min
#define VMA_USE_STL_CONTAINERS 0
#define VMA_USE_STL_VECTOR 0
#define VMA_USE_STL_UNORDERED_MAP 0
#define VMA_USE_STL_LIST 0

#if defined (_MSC_VER)
#pragma warning (disable: 4127)
#pragma warning (disable: 4189)
#pragma warning (disable: 4191)
#pragma warning (disable: 4296)
#pragma warning (disable: 4324)
#pragma warning (disable: 4355)
#pragma warning (disable: 4365)
#pragma warning (disable: 4625)
#pragma warning (disable: 4668)
#pragma warning (disable: 5026)
#pragma warning (disable: 5027)
#endif

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "vulkan/vulkan.h"

#include "GLFW/glfw3.h"

namespace puffin {

static void check_result(VkResult result);

#define                 check(result) PASSERTM(result == VK_SUCCESS, "Vulkan assert code %u", result)

struct CommandBufferRing {

    void init(GpuDevice* gpu);

    void shutdown();

    void reset_pools(u32 frame_index);

    CommandBuffer* get_command_buffer(u32 frame, bool begin);

    CommandBuffer* get_command_buffer_instant(u32 frame, bool begin);

    static u16 pool_from_index(u32 index) { return (u16) index / k_buffer_per_pool; }


    static const u16 k_max_threads = 1;
    static const u16 k_max_pools = k_max_swapchain_images * k_max_threads;
    static const u16 k_buffer_per_pool = 4;
    static const u16 k_max_buffers = k_buffer_per_pool * k_max_pools;

    GpuDevice* gpu;
    VkCommandPool vulkan_command_pools[k_max_pools];
    CommandBuffer command_buffers[k_max_buffers];
    u8 next_free_per_thread_frame[k_max_pools];
};

void CommandBufferRing::init(GpuDevice* gpu_) {
    gpu = gpu_;

    for (u32 i = 0; i < k_max_pools; i++) {
        VkCommandPoolCreateInfo cmd_pool_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr};
        cmd_pool_info.queueFamilyIndex = gpu->vulkan_queue_family;
        cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        check(vkCreateCommandPool(gpu->vulkan_device, &cmd_pool_info, gpu->vulkan_allocation_callbacks,
                                  &vulkan_command_pools[i]));
    }

    for (u32 i = 0; i < k_max_buffers; i++) {
        VkCommandBufferAllocateInfo cmd = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr};
        const u32 pool_index = pool_from_index(i);
        cmd.commandPool = vulkan_command_pools[pool_index];
        cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd.commandBufferCount = 1;
        check(vkAllocateCommandBuffers(gpu->vulkan_device, &cmd, &command_buffers[i].vk_command_buffer));

        command_buffers[i].gpu_device = gpu;
        command_buffers[i].init(QueueType::Enum::Graphics, 0, 0, false);
        command_buffers[i].reset();
    }
}

void CommandBufferRing::shutdown() {
    for (u32 i = 0; i < k_max_swapchain_images * k_max_threads; i++) {
        vkDestroyCommandPool(gpu->vulkan_device, vulkan_command_pools[i], gpu->vulkan_allocation_callbacks);
    }
}

void CommandBufferRing::reset_pools(u32 frame_index) {
    for (u32 i = 0; i < k_max_threads; i++) {
        vkResetCommandPool(gpu->vulkan_device, vulkan_command_pools[frame_index * k_max_threads + i], 0);
    }
}

CommandBuffer* CommandBufferRing::get_command_buffer(u32 frame, bool begin) {
    CommandBuffer* cb = &command_buffers[frame * k_buffer_per_pool];

    if (begin) {
        cb->reset();

        VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb->vk_command_buffer, &begin_info);
    }

    return cb;
}

CommandBuffer* CommandBufferRing::get_command_buffer_instant(u32 frame, bool begin) {
    CommandBuffer* cb = &command_buffers[frame * k_buffer_per_pool + 1];
    return cb;
}

// Device Implementation

// Methods

// Enable for debugging
#define VULKAN_DEBUG_REPORT
#define VULKAN_SYNCHRONIZATION_VALIDATION

static cstring s_requested_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        // platform specific extension !! only defining for WIN32 for now
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#if defined(VULKAN_DEBUG_REPORT)
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
};

static cstring s_requested_layers[] = {
#if defined(VULKAN_DEBUG_REPORT)
      "VK_LAYER_KHRONOS_validation",
#else
        ""
#endif
};

#ifdef VULKAN_DEBUG_REPORT

static VkBool32 debug_utils_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                     VkDebugUtilsMessageTypeFlagsEXT types,
                                     const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                                     void* user_data) {
    p_print(" Message ID: %s %i\nMessage: %s\n\n", callback_data->pMessageIdName, callback_data->messageIdNumber, callback_data->pMessage);
    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT create_debug_utils_messenger_info() {
    VkDebugUtilsMessengerCreateInfoEXT creation_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    creation_info.pfnUserCallback = debug_utils_callback;
    creation_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    creation_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

    return creation_info;
}

#endif // vulkan debug report

// GPU Timestamps
static GLFWwindow*                  glfw_window;
PFN_vkSetDebugUtilsObjectNameEXT    pfnSetDebugUtilsObjectNameEXT;
PFN_vkCmdBeginDebugUtilsLabelEXT    pfnCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT      pfnCmdEndDebugUtilsLabelEXT;

static puffin::FlatHashMap<u64, VkRenderPass> render_pass_cache;
static CommandBufferRing            command_buffer_ring;
static size_t                       s_ubo_alignment     = 256;
static size_t                       s_ssbo_alignment    = 256;

static const u32                    k_bindless_texture_binding  = 10;
static const u32                    k_max_bindless_resources    = 1024;

void GpuDevice::init(const DeviceCreation& creation) {
    p_print("Gpu Device init\n");

    // 1. Common code
    allocator = creation.allocator;
    temporary_allocator = creation.temporary_allocator;
    string_buffer.init( creation.allocator, 1024 * 1024);

    // Init vulkan instance
    VkResult result;
    vulkan_allocation_callbacks = nullptr;
    VkApplicationInfo application_info = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO,
            nullptr,
            "Puffin Graphics Device", 1,
            "Puffin", 1,
            VK_MAKE_VERSION(1, 2, 0)
    };

    VkInstanceCreateInfo create_info = {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            nullptr,
            0,
            &application_info,
#ifdef VULKAN_DEBUG_REPORT
            PuffinArraySize(s_requested_layers),
            s_requested_layers,
#else
            0,
            nullptr,
#endif
            PuffinArraySize(s_requested_extensions),
            s_requested_extensions
    };

#ifdef VULKAN_DEBUG_REPORT
    const VkDebugUtilsMessengerCreateInfoEXT debug_create_info = create_debug_utils_messenger_info();
#ifdef VULKAN_SYNCHRONIZATION_VALIDATION
    const VkValidationFeatureEnableEXT features_requested[] = {
            VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
            VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
    };
    VkValidationFeaturesEXT features = {
            VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
            &debug_create_info,
            PuffinArraySize(features_requested),
            features_requested
    };
    create_info.pNext = &features;
#else
    create_info.pNext = &debug_create_info;
#endif // VULKAN_SYNCHRONIZATION_VALIDATION
#endif // VULKAN_DEBUG_REPORT


    // Create vulkan instance
    result = vkCreateInstance(&create_info, vulkan_allocation_callbacks, &vulkan_instance);
    check(result);

    swapchain_width = creation.width;
    swapchain_height = creation.height;

    // Choose extensions
#ifdef VULKAN_DEBUG_REPORT
    {
        u32 num_instance_extensions;
        vkEnumerateInstanceExtensionProperties(nullptr, &num_instance_extensions, nullptr);
        VkExtensionProperties* extensions = (VkExtensionProperties*) puffin_alloc(sizeof(VkExtensionProperties) * num_instance_extensions, allocator);
        vkEnumerateInstanceExtensionProperties(nullptr, &num_instance_extensions, extensions);
        for(size_t i = 0; i < num_instance_extensions; i++) {
            if(!strcmp(extensions[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                debug_utils_extension_present = true;
                break;
            }
        }

        puffin_free(extensions, allocator);

        if(!debug_utils_extension_present) {
          p_print("Extension %s for debugging not present", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        } else {
            // Create debug callback
            PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vulkan_instance, "vkCreateDebugUtilsMessengerEXT");
            VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = create_debug_utils_messenger_info();

            vkCreateDebugUtilsMessengerEXT(vulkan_instance, &debug_messenger_create_info, vulkan_allocation_callbacks, &vulkan_debug_utils_messenger);
        }
    };
#endif

    // Choose physical device -- TODO, use Sascha Willems's gpu selection code
    u32 num_physical_devices;
    result = vkEnumeratePhysicalDevices(vulkan_instance, &num_physical_devices, NULL);
    check(result);

    VkPhysicalDevice* gpus = (VkPhysicalDevice*) puffin_alloc(sizeof(VkPhysicalDevice) * num_physical_devices, allocator);
    result = vkEnumeratePhysicalDevices(vulkan_instance, &num_physical_devices, gpus);
    check(result);

    vulkan_physical_device = gpus[0];
    puffin_free(gpus, allocator);

    vkGetPhysicalDeviceProperties(vulkan_physical_device, &vulkan_physical_device_properties);
    gpu_timestamp_frequency = vulkan_physical_device_properties.limits.timestampPeriod / (1000 * 1000);

    p_print("GPU Used: %s\n", vulkan_physical_device_properties.deviceName);

    s_ubo_alignment = vulkan_physical_device_properties.limits.minUniformBufferOffsetAlignment;
    s_ssbo_alignment = vulkan_physical_device_properties.limits.minUniformBufferOffsetAlignment;

    VkPhysicalDeviceDescriptorIndexingFeatures indexing_features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
                                                                  nullptr};
    VkPhysicalDeviceFeatures2 device_features {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &indexing_features};
    vkGetPhysicalDeviceFeatures2(vulkan_physical_device, &device_features);

    // Partially binding means that the descriptor (pointer) does not need to point to anything valid if they aren't
    // dynamically used (accessed by a shader)
    bindless_supported = indexing_features.descriptorBindingPartiallyBound && indexing_features.runtimeDescriptorArray;

    // Create logical device
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vulkan_physical_device, &queue_family_count, nullptr);

    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*) puffin_alloc(sizeof(VkQueueFamilyProperties) * queue_family_count, allocator);
    vkGetPhysicalDeviceQueueFamilyProperties(vulkan_physical_device, &queue_family_count, queue_families);

    u32 family_index = 0;
    for( ; family_index < queue_family_count; family_index++) {
        VkQueueFamilyProperties queue_family = queue_families[family_index];
        if(queue_family.queueCount > 0 && queue_family.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
            break;
        }
    }

    puffin_free(queue_families, allocator);

    u32 device_extension_count = 1;
    cstring device_extensions[] = { "VK_KHR_swapchain" };
    const float queue_priority[] = {1.0f };
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = family_index;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;

    // Enable all features: just pass the physical features to struct
    VkPhysicalDeviceFeatures2 physical_features_2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    vkGetPhysicalDeviceFeatures2(vulkan_physical_device, &physical_features_2);

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = PuffinArraySize(queue_info);
    device_create_info.pQueueCreateInfos = queue_info;
    device_create_info.enabledExtensionCount = device_extension_count;
    device_create_info.ppEnabledExtensionNames = device_extensions;
    device_create_info.pNext = &physical_features_2;

    if(bindless_supported) {
        physical_features_2.pNext = &indexing_features;
    }

    result = vkCreateDevice(vulkan_physical_device, &device_create_info, vulkan_allocation_callbacks, &vulkan_device);
    check(result);

    // Get the function pointers to debug utils functions
    if(debug_utils_extension_present) {
        pfnSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT) vkGetDeviceProcAddr(vulkan_device, "vkSetDebugUtilsObjectNameEXT");
        pfnCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT) vkGetDeviceProcAddr(vulkan_device, "vkCmdBeginDebugUtilsLabelEXT");
        pfnCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT) vkGetDeviceProcAddr(vulkan_device, "vkCmdEndDebugUtilsLabelEXT");
    }

    vkGetDeviceQueue(vulkan_device, family_index, 0, &vulkan_queue);

    vulkan_queue_family = family_index;

    // Create drawable surface
    GLFWwindow* window = (GLFWwindow*) creation.window;

    result = glfwCreateWindowSurface(vulkan_instance, window, NULL, &vulkan_window_surface);
    check(result);

    glfw_window = window;

    // Create Framebuffers
    int window_width, window_height;
    glfwGetWindowSize(glfw_window, &window_width, &window_height);

    // Select Surface Format
    const VkFormat surface_image_formats[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_R8G8B8_UNORM
    };

    const VkColorSpaceKHR surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    u32 supported_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan_physical_device, vulkan_window_surface, &supported_count, NULL);
    VkSurfaceFormatKHR* supported_formats = (VkSurfaceFormatKHR*)puffin_alloc(sizeof(VkSurfaceFormatKHR) * supported_count, allocator);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan_physical_device, vulkan_window_surface, &supported_count, supported_formats);

    // Cache render pass output
    swapchain_output.reset();

    // Check for supported formats
    bool format_found = false;
    const u32 surface_format_count = PuffinArraySize(surface_image_formats);

    for(int i = 0; i < surface_format_count; i++) {
        for(int j = 0; j < supported_count; j++) {
            if(surface_image_formats[i] == supported_formats[j].format && supported_formats[j].colorSpace == surface_color_space) {
                vulkan_surface_format = supported_formats[j];
                swapchain_output.color(supported_formats[j].format); // This line is wrong in the book
                format_found = true;
                break;
            }
        }

        if(format_found) {
            break;
        }
    }

    // Default to the first format supported
    if(!format_found) {
        vulkan_surface_format = supported_formats[0];
        PASSERT(false);
    }
    puffin_free(supported_formats, allocator);

    set_present_mode(present_mode);

    // Create swapchain
    create_swapchain();

    // Create VMA allocator
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = vulkan_physical_device;
    allocator_info.device = vulkan_device;
    allocator_info.instance = vulkan_instance;

    result = vmaCreateAllocator(&allocator_info, &vma_allocator);
    check(result);

    // Create pools
    static const u32 k_global_pool_elements = 128;
    VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, k_global_pool_elements },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, k_global_pool_elements },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, k_global_pool_elements },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, k_global_pool_elements },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, k_global_pool_elements },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, k_global_pool_elements },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, k_global_pool_elements },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, k_global_pool_elements },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, k_global_pool_elements },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, k_global_pool_elements },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, k_global_pool_elements },
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = k_global_pool_elements * PuffinArraySize(pool_sizes);
    pool_info.poolSizeCount = (u32) PuffinArraySize(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    result = vkCreateDescriptorPool(vulkan_device, &pool_info, vulkan_allocation_callbacks, &vulkan_descriptor_pool);
    check(result);

    if(bindless_supported) {
        VkDescriptorPoolSize pool_sizes_bindless[] = {
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, k_max_bindless_resources},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, k_max_bindless_resources},
        };

        // Update after bind is needed here, for each binding, and the descriptor set layout creation
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
        pool_info.maxSets = k_max_bindless_resources * PuffinArraySize(pool_sizes_bindless);
        pool_info.poolSizeCount = (u32) PuffinArraySize(pool_sizes_bindless);
        pool_info.pPoolSizes = pool_sizes_bindless;
        result = vkCreateDescriptorPool(vulkan_device, &pool_info, vulkan_allocation_callbacks, &vulkan_bindless_descriptor_pool);
        check(result);

        const u32 pool_count = (u32) PuffinArraySize(pool_sizes_bindless);
        VkDescriptorSetLayoutBinding vk_binding[4];

        VkDescriptorSetLayoutBinding& image_sampler_binding = vk_binding[0];
        image_sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        image_sampler_binding.descriptorCount = k_max_bindless_resources;
        image_sampler_binding.binding = k_bindless_texture_binding;
        image_sampler_binding.stageFlags = VK_SHADER_STAGE_ALL;
        image_sampler_binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding& storage_image_binding = vk_binding[1];
        storage_image_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        storage_image_binding.descriptorCount = k_max_bindless_resources;
        storage_image_binding.binding = k_bindless_texture_binding + 1;
        storage_image_binding.stageFlags = VK_SHADER_STAGE_ALL;
        storage_image_binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layout_info.bindingCount = pool_count;
        layout_info.pBindings = vk_binding;
        layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;

        VkDescriptorBindingFlags bindless_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
        VkDescriptorBindingFlags binding_flags[4];

        binding_flags[0] = bindless_flags;
        binding_flags[1] = bindless_flags;

        VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT, nullptr };
        extended_info.bindingCount = pool_count;
        extended_info.pBindingFlags = binding_flags;

        layout_info.pNext = &extended_info;

        vkCreateDescriptorSetLayout(vulkan_device, &layout_info, vulkan_allocation_callbacks, &vulkan_bindless_descriptor_set_layout);

        VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        alloc_info.descriptorPool = vulkan_bindless_descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &vulkan_bindless_descriptor_set_layout;

        VkDescriptorSetVariableDescriptorCountAllocateInfoEXT count_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT };
        u32 max_binding = k_max_bindless_resources - 1;
        count_info.descriptorSetCount = 1;
        count_info.pDescriptorCounts = &max_binding;

        result = vkAllocateDescriptorSets(vulkan_device, &alloc_info, &vulkan_bindless_descriptor_set);
        check(result);
    }

    // Create timestamp query pool used for GPU timings
    VkQueryPoolCreateInfo query_pool_create_info = {
            VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            nullptr,
            0,
            VK_QUERY_TYPE_TIMESTAMP,
            creation.gpu_time_queries_per_frame * 2 * k_max_frames,
            0
    };
    vkCreateQueryPool(vulkan_device, &query_pool_create_info, vulkan_allocation_callbacks, &vulkan_timestamp_query_pool);

    // Init pools
    buffers.init(allocator, 512, sizeof(Buffer));
    textures.init(allocator, 512, sizeof(Texture));
    render_passes.init(allocator, 256, sizeof(RenderPass));
    descriptor_set_layouts.init(allocator, 128, sizeof(DescriptorSetLayout));
    pipelines.init(allocator, 128, sizeof(Pipeline));
    shaders.init(allocator, 128, sizeof(ShaderState));
    descriptor_sets.init(allocator, 128, sizeof(DescriptorSet));
    samplers.init(allocator, 32, sizeof(Sampler));

    // Init render frame information. Includes fences, semaphores, command buffer, etc.
    u8* memory = puffin_alloc_return_mem_pointer(sizeof(GPUTimestampManager) + sizeof(CommandBuffer*) * 128, allocator);

    VkSemaphoreCreateInfo semaphore_info = {
            VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    vkCreateSemaphore(vulkan_device, &semaphore_info, vulkan_allocation_callbacks, &vulkan_image_acquired_semaphore);

    for(size_t swapchain_index = 0; swapchain_index < k_max_swapchain_images; swapchain_index++) {
        vkCreateSemaphore(vulkan_device, &semaphore_info, vulkan_allocation_callbacks, &vulkan_render_complete_semaphore[swapchain_index]);

        VkFenceCreateInfo fence_info = {
                VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
        };
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(vulkan_device, &fence_info, vulkan_allocation_callbacks, &vulkan_command_buffer_executed_fence[swapchain_index]);
    }

    gpu_timestamp_manager = (GPUTimestampManager*)(memory);
    gpu_timestamp_manager->init(allocator, creation.gpu_time_queries_per_frame, k_max_frames);

    command_buffer_ring.init(this);

    // Allocate queued command buffers array
    queued_command_buffers = (CommandBuffer**)(gpu_timestamp_manager + 1);
    CommandBuffer** correctly_allocated_buffer = (CommandBuffer**)(memory + sizeof(GPUTimestampManager));
    PASSERTM(queued_command_buffers == correctly_allocated_buffer, "Wrong calculations for queue command buffers arrays. Should be %p, but is %p.", correctly_allocated_buffer,
             queued_command_buffers);

    vulkan_image_index = 0;
    current_frame = 1;
    previous_frame = 0;
    absolute_frame = 0;
    timestamps_enabled = false;

    resource_deletion_queue.init(allocator, 16);
    descriptor_set_updates.init(allocator, 16);
    texture_to_update_bindless.init(allocator, 16);

    // Init primitive resources
    SamplerCreation sc{};
    sc.set_address_mode_uvw(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
        .set_min_mag_mip(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR)
        .set_name("Default Sampler");
    default_sampler = create_sampler(sc);

    BufferCreation fullscreen_vb_creation = {
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            ResourceUsageType::Immutable,
            0,
            nullptr,
            "Fullscreen_vb"
    };
    fullscreen_vertex_buffer = create_buffer(fullscreen_vb_creation);

    // Create depth image
    TextureCreation depth_texture_creation = {
            nullptr,
            swapchain_width,
            swapchain_height,
            1,
            1,
            0,
            VK_FORMAT_D32_SFLOAT,
            TextureType::Texture2D,
            "DepthImage_Texture"
    };
    depth_texture = create_texture(depth_texture_creation);

    // Cache depth texture format
    swapchain_output.depth(VK_FORMAT_D32_SFLOAT);

    RenderPassCreation swapchain_pass_creation = {};
    swapchain_pass_creation.set_type(RenderPassType::Swapchain).set_name("Swapchain");
    swapchain_pass_creation.set_operations(RenderPassOperation::Clear, RenderPassOperation::Clear, RenderPassOperation::Clear);
    swapchain_pass = create_render_pass(swapchain_pass_creation);

    // Init dummy resources
    TextureCreation dummy_texture_creation = {
            nullptr,
            1, 1, 1, 1, 0,
            VK_FORMAT_R8_UINT, TextureType::Texture2D
    };
    dummy_texture = create_texture(dummy_texture_creation);

    BufferCreation dummy_constant_buffer_creation = {
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            ResourceUsageType::Immutable, 16,
            nullptr, "dummy_cb"
    };
    dummy_constant_buffer = create_buffer(dummy_constant_buffer_creation);

    // Get binary's paths
#ifdef _MSC_VER
    char* vulkan_env = string_buffer.reserve(512);
    ExpandEnvironmentStringsA("%VULKAN_SDK%", vulkan_env, 512);
    char* compiler_path = string_buffer.append_use_f("%s\\Bin\\", vulkan_env);
#else
    char* vulkan_env = get_env("VULKAN_SDK");
    char* compiler_path = string_buffer.append_use_f("%s/bin/", vulkan_env);
#endif

    strcpy(vulkan_binaries_path, compiler_path);
    string_buffer.clear();

    // Dynamic buffer handling
    dynamic_per_frame_size = 1024 * 1024 * 10;
    BufferCreation bc;
    bc.set(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            ResourceUsageType::Immutable,
            dynamic_per_frame_size * k_max_frames)
            .set_name("Dynamic_Persistent_Buffer");
    dynamic_buffer = create_buffer(bc);

    MapBufferParameters cb_map = {dynamic_buffer, 0, 0};
    dynamic_mapped_memory = (u8*)map_buffer(cb_map);

    // Init render pass cache
    render_pass_cache.init(allocator, 16);
}

void GpuDevice::shutdown() {
    vkDeviceWaitIdle(vulkan_device);

    command_buffer_ring.shutdown();

    for(size_t i = 0; i < k_max_swapchain_images; i++) {
        vkDestroySemaphore(vulkan_device, vulkan_render_complete_semaphore[i], vulkan_allocation_callbacks);
        vkDestroyFence(vulkan_device, vulkan_command_buffer_executed_fence[i], vulkan_allocation_callbacks);
    }

    vkDestroySemaphore(vulkan_device, vulkan_image_acquired_semaphore, vulkan_allocation_callbacks);

    gpu_timestamp_manager->shutdown();

    MapBufferParameters cb_map = {dynamic_buffer, 0, 0};
    unmap_buffer(cb_map);

    // Memory: this contains allocations for gpu timestamp memory, queued command buffers and render frames
    puffin_free(gpu_timestamp_manager, allocator);

    destroy_texture(depth_texture);
    destroy_buffer(fullscreen_vertex_buffer);
    destroy_buffer(dynamic_buffer);
    destroy_render_pass(swapchain_pass);
    destroy_texture(dummy_texture);
    destroy_buffer(dummy_constant_buffer);
    destroy_sampler(default_sampler);

    // Destroy pending resources
    for(u32 i = 0; i < resource_deletion_queue.size; i++) {
        ResourceUpdate& resource_deletion = resource_deletion_queue[i];

        // Skip just free resources
        if(resource_deletion.current_frame == -1){
            continue;
        }

        switch(resource_deletion.type) {
            case ResourceDeletionType::Buffer:
            {
                destroy_buffer_instant(resource_deletion.handle);
                break;
            }

            case ResourceDeletionType::Pipeline:
            {
                destroy_pipeline_instant(resource_deletion.handle);
                break;
            }

            case ResourceDeletionType::RenderPass:
            {
                destroy_render_pass_instant(resource_deletion.handle);
                break;
            }

            case ResourceDeletionType::DescriptorSet:
            {
                destroy_descriptor_set_instant(resource_deletion.handle);
                break;
            }

            case ResourceDeletionType::DescriptorSetLayout:
            {
                destroy_descriptor_set_layout_instant(resource_deletion.handle);
                break;
            }

            case ResourceDeletionType::ShaderState:
            {
                destroy_shader_state_instant(resource_deletion.handle);
                break;
            }

            case ResourceDeletionType::Texture:
            {
                destroy_texture_instant(resource_deletion.handle);
                break;
            }
        }
    }

    // Destroy render passes from the cache
    FlatHashMapIterator it = render_pass_cache.iterator_begin();
    while(it.is_valid()) {
        VkRenderPass vk_render_pass = render_pass_cache.get(it);
        vkDestroyRenderPass(vulkan_device, vk_render_pass, vulkan_allocation_callbacks);
        render_pass_cache.iterator_advance(it);
    }
    render_pass_cache.shutdown();

    // Destroy swapchain render pass, not present in cache
    RenderPass* vk_swapchain_pass = access_render_pass(swapchain_pass);
    vkDestroyRenderPass(vulkan_device, vk_swapchain_pass->vk_render_pass, vulkan_allocation_callbacks);

    // Destroy swapchain
    destroy_swapchain();
    vkDestroySurfaceKHR(vulkan_instance, vulkan_window_surface, vulkan_allocation_callbacks);

    vmaDestroyAllocator(vma_allocator);

    texture_to_update_bindless.shutdown();
    resource_deletion_queue.shutdown();
    descriptor_set_updates.shutdown();

    pipelines.shutdown();
    buffers.shutdown();
    shaders.shutdown();
    textures.shutdown();
    samplers.shutdown();
    descriptor_set_layouts.shutdown();
    descriptor_sets.shutdown();
    render_passes.shutdown();

#ifdef VULKAN_DEBUG_REPORT
    // Remove debug report callback
    auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(vulkan_instance, "vkDestroyDebugUtilsMessengerEXT");
    vkDestroyDebugUtilsMessengerEXT(vulkan_instance, vulkan_debug_utils_messenger, vulkan_allocation_callbacks);
#endif

    vkDestroyDescriptorPool(vulkan_device, vulkan_descriptor_pool, vulkan_allocation_callbacks);
    vkDestroyQueryPool(vulkan_device, vulkan_timestamp_query_pool, vulkan_allocation_callbacks);

    vkDestroyDevice(vulkan_device, vulkan_allocation_callbacks);

    vkDestroyInstance(vulkan_instance, vulkan_allocation_callbacks);

    string_buffer.shutdown();

    p_print("GPU Device shutdown\n");
}

static void transition_image_layout(VkCommandBuffer command_buffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, bool is_depth) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange.aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        // p_print("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(command_buffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// Resource Creation
static void vulkan_create_texture(GpuDevice& gpu, const TextureCreation& creation, TextureHandle handle, Texture* texture) {
    texture->width = creation.width;
    texture->height = creation.height;
    texture->depth = creation.depth;
    texture->mipmaps = creation.mipmaps;
    texture->type = creation.type;
    texture->name = creation.name;
    texture->vk_format = creation.format;
    texture->sampler = nullptr;
    texture->flags = creation.flags;

    texture->handle = handle;

    // Create the image
    VkImageCreateInfo image_info = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
    };
    image_info.format = texture->vk_format;
    image_info.flags = 0;
    image_info.imageType = to_vk_image_type(creation.type);
    image_info.extent.width = creation.width;
    image_info.extent.height = creation.height;
    image_info.extent.depth = creation.depth;
    image_info.mipLevels = creation.mipmaps;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;

    const bool is_render_target = (creation.flags & TextureFlags::RenderTarget_mask) == TextureFlags::RenderTarget_mask;
    const bool is_compute_used = (creation.flags & TextureFlags::Compute_mask) == TextureFlags::Compute_mask;

    // Default to always readable from shader
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    image_info.usage |= is_compute_used ? VK_IMAGE_USAGE_STORAGE_BIT : 0;

    if(TextureFormat::has_depth_or_stencil(creation.format)) {
        // Depth/stencil textures are normally textures you render into
        image_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    } else {
        image_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_info.usage |= is_render_target ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
    }

    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo memory_info{};
    memory_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult result = vmaCreateImage(gpu.vma_allocator, &image_info, &memory_info,
                                     &texture->vk_image, &texture->vma_allocation, nullptr);
    check(result);

    gpu.set_resource_name(VK_OBJECT_TYPE_IMAGE, (u64)texture->vk_image, creation.name);

    // Create the image view
    VkImageViewCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    info.image = texture->vk_image;
    info.viewType = to_vk_image_view_type(creation.type);
    info.format = image_info.format;

    if(TextureFormat::has_depth_or_stencil(creation.format)) {
        info.subresourceRange.aspectMask = TextureFormat::has_depth(creation.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
    } else {
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    info.subresourceRange.levelCount = 1;
    info.subresourceRange.layerCount = 1;

    result = vkCreateImageView(gpu.vulkan_device, &info, gpu.vulkan_allocation_callbacks, &texture->vk_image_view);
    check(result);

    gpu.set_resource_name(VK_OBJECT_TYPE_IMAGE_VIEW, (u64)texture->vk_image_view, creation.name);
    texture->vk_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    if(gpu.bindless_supported) {
        ResourceUpdate resource_update { ResourceDeletionType::Texture, texture->handle.index, gpu.current_frame };
        gpu.texture_to_update_bindless.push(resource_update);
    }
}

TextureHandle GpuDevice::create_texture(const TextureCreation& creation) {
    u32 resource_index = textures.obtain_resource();
    TextureHandle handle = {resource_index};

    if(resource_index == k_invalid_index) {
        return handle;
    }

    Texture* texture = access_texture(handle);

    vulkan_create_texture(*this, creation, handle, texture);

    // Copy buffer_data if present
    if(creation.initial_data) {
        // Create staging buffer
        VkBufferCreateInfo buffer_info = {
                VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO
        };
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        u32 image_size = creation.width * creation.height * 4;
        buffer_info.size = image_size;

        VmaAllocationCreateInfo memory_info{};
        memory_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
        memory_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VmaAllocationInfo allocation_info{};
        VkBuffer staging_buffer;
        VmaAllocation staging_allocation;

        VkResult result = vmaCreateBuffer(vma_allocator, &buffer_info, &memory_info,
                                          &staging_buffer, &staging_allocation, &allocation_info);
        check(result);

        void* destination_data;
        vmaMapMemory(vma_allocator, staging_allocation, &destination_data);
        memcpy(destination_data, creation.initial_data, static_cast<size_t>(image_size));
        vmaUnmapMemory(vma_allocator, staging_allocation);

        VkCommandBufferBeginInfo beginInfo = {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        CommandBuffer* command_buffer = get_instant_command_buffer();
        vkBeginCommandBuffer(command_buffer->vk_command_buffer, &beginInfo);

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {creation.width, creation.height, creation.depth};

        // Transition
        transition_image_layout(command_buffer->vk_command_buffer, texture->vk_image, texture->vk_format,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, false);

        // Copy
        vkCmdCopyBufferToImage(command_buffer->vk_command_buffer, staging_buffer, texture->vk_image,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Transition
        transition_image_layout(command_buffer->vk_command_buffer, texture->vk_image, texture->vk_format,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);

        vkEndCommandBuffer(command_buffer->vk_command_buffer);

        // Submit command buffer
        VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO
        };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &command_buffer->vk_command_buffer;

        vkQueueSubmit(vulkan_queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(vulkan_queue);

        vmaDestroyBuffer(vma_allocator, staging_buffer, staging_allocation);

        vkResetCommandBuffer(command_buffer->vk_command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

        texture->vk_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    return handle;
}

// helper method
bool is_end_of_line(char c) {
    bool result = (( c == '\n' ) || ( c == '\r' ));
    return result;
}

void dump_shader_code(StringBuffer& temp_string_buffer, cstring code, VkShaderStageFlagBits stage, cstring name) {
    p_print("Error in creation of shader %s, stage %s. Writing shader:\n", name, to_stage_defines(stage));

    cstring current_code = code;
    u32 line_index = 1;
    while(current_code) {
        cstring end_of_line = current_code;
        if(!end_of_line || *end_of_line == 0) {
            break;
        }
        while(!is_end_of_line(*end_of_line)) {
            end_of_line++;
        }

        if(*end_of_line == '\r') {
            end_of_line++;
        }
        if(*end_of_line == '\n') {
            end_of_line++;
        }

        temp_string_buffer.clear();
        char* line = temp_string_buffer.append_use_substring(current_code, 0, (end_of_line - current_code));
        p_print("%u: %s", line_index++, line);

        current_code = end_of_line;
    }
}

VkShaderModuleCreateInfo GpuDevice::compile_shader(cstring code, u32 code_size, VkShaderStageFlagBits stage, cstring name) {
    VkShaderModuleCreateInfo shader_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
    };

    p_print("compiling shader %s\n", name);

    // Compile from glsl to spirv
    cstring temp_filename = "temp.shader";

    // Write current shader to file
    FILE* temp_shader_file = fopen(temp_filename, "w");
    fwrite(code, code_size, 1, temp_shader_file);
    fclose(temp_shader_file);

    size_t current_marker = temporary_allocator->get_marker();
    StringBuffer temp_string_buffer;
    temp_string_buffer.init(temporary_allocator, puffin_kilo(1));

    // Add uppercase define as STAGE_NAME
    char* stage_define = temp_string_buffer.append_use_f("%s_%s", to_stage_defines(stage), name);
    size_t stage_define_length = strlen(stage_define);
    for(u32 i = 0; i < stage_define_length; i++) {
        stage_define[i] = toupper(stage_define[i]);
    }

    // Compile to SPV
    char* glsl_compiler_path = temp_string_buffer.append_use_f("%sglslangValidator.exe", vulkan_binaries_path);
    char* final_spirv_filename = temp_string_buffer.append_use("shader_final.spv");
    char* arguments = temp_string_buffer.append_use_f("glslangValidator.exe %s -V --target-env vulkan1.2 -o %s -S %s --D %s --D %s",
                       temp_filename, final_spirv_filename, to_compiler_extension(stage), stage_define, to_stage_defines(stage));

    process_execute(".", glsl_compiler_path, arguments, "");

    bool optimize_shaders = false;

    if(optimize_shaders) {
        char* spirv_optimizer_path = temp_string_buffer.append_use_f("%sspirv-opt.exe", vulkan_binaries_path);
        char* optimized_spirv_filename = temp_string_buffer.append_use_f("shader_opt.spv");
        char* spirv_opt_arguments = temp_string_buffer.append_use_f("spirv-opt.exe -O --preserve-bindings %s -o %s",
                                                                    final_spirv_filename, optimized_spirv_filename);

        process_execute(".", spirv_optimizer_path, spirv_opt_arguments, "");

        shader_create_info.pCode = reinterpret_cast<const u32*>(file_read_binary(optimized_spirv_filename, temporary_allocator, &shader_create_info.codeSize));

        file_delete(optimized_spirv_filename);
    } else {
        shader_create_info.pCode = reinterpret_cast<const u32*>(file_read_binary(final_spirv_filename, temporary_allocator, &shader_create_info.codeSize));
    }

    // Handling compilation error
    if(shader_create_info.pCode == nullptr) {
        dump_shader_code(temp_string_buffer, code, stage, name);
    }

    file_delete(temp_filename);
    file_delete(final_spirv_filename);

    return shader_create_info;
}

ShaderStateHandle GpuDevice::create_shader_state(const ShaderStateCreation& creation) {
    ShaderStateHandle handle = { k_invalid_index };

    if(creation.stages_count == 0 || creation.stages == nullptr) {
        p_print("Shader %s does not contain shader stages.\n", creation.name);
        return handle;
    }

    handle.index = shaders.obtain_resource();
    if(handle.index == k_invalid_index) {
        return handle;
    }

    // For each shader stage, compile them individually
    u32 compiled_shaders = 0;

    ShaderState* shader_state = access_shader_state(handle);
    shader_state->graphics_pipeline = true;
    shader_state->active_shaders = 0;

    size_t current_temporary_marker = temporary_allocator->get_marker();

    StringBuffer name_buffer;
    name_buffer.init(temporary_allocator, 4096);

    shader_state->parse_result = (spirv::ParseResult*)allocator->allocate(sizeof(spirv::ParseResult), 64);
    memset(shader_state->parse_result, 0, sizeof(spirv::ParseResult));

    for(compiled_shaders = 0; compiled_shaders < creation.stages_count; compiled_shaders++) {
        const ShaderStage& stage = creation.stages[compiled_shaders];

        // Gives priority to compute: if any is present (and it shouldn't be) then it is not a graphics problem
        if(stage.type == VK_SHADER_STAGE_COMPUTE_BIT) {
            shader_state->graphics_pipeline = false;
        }

        VkShaderModuleCreateInfo shader_create_info = {
                VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
        };

        if(creation.spv_input) {
            shader_create_info.codeSize = stage.code_size;
            shader_create_info.pCode = reinterpret_cast<const u32*>(stage.code);
        } else {
            shader_create_info = compile_shader(stage.code, stage.code_size, stage.type, creation.name);
        }

        // Compile shader module
        VkPipelineShaderStageCreateInfo& shader_stage_info = shader_state->shader_stage_info[compiled_shaders];
        memset(&shader_stage_info, 0, sizeof(VkPipelineShaderStageCreateInfo));
        shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_info.pName = "main";
        shader_stage_info.stage = stage.type;

        if(vkCreateShaderModule(vulkan_device, &shader_create_info, nullptr,
                                &shader_state->shader_stage_info[compiled_shaders].module) != VK_SUCCESS) {
            break;
        }

        spirv::parse_binary(shader_create_info.pCode, shader_create_info.codeSize, name_buffer, shader_state->parse_result);

        set_resource_name(VK_OBJECT_TYPE_SHADER_MODULE, (u64)shader_state->shader_stage_info[compiled_shaders].module, creation.name);
    }

    temporary_allocator->free_marker(current_temporary_marker);

    bool creation_failed = compiled_shaders != creation.stages_count;
    if(!creation_failed) {
        shader_state->active_shaders = compiled_shaders;
        shader_state->name = creation.name;
    }

    if(creation_failed) {
        destroy_shader_state(handle);
        handle.index = k_invalid_index;

        p_print("Error in creation of shader %s. Dumping all shader informations.\n", creation.name);
        for(compiled_shaders = 0; compiled_shaders < creation.stages_count; compiled_shaders++) {
            const ShaderStage& stage = creation.stages[compiled_shaders];
            p_print("%u:\n%s\n", stage.type, stage.code);
        }
    }

    return handle;
}

PipelineHandle GpuDevice::create_pipeline(const PipelineCreation& creation, cstring cache_path) {
    PipelineHandle handle = { pipelines.obtain_resource() };
    if(handle.index == k_invalid_index) {
        return handle;
    }

    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
    VkPipelineCacheCreateInfo pipeline_cache_create_info { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };

    bool cache_exists = file_exists(cache_path);
    if(cache_path != nullptr && cache_exists) {
        FileReadResult read_result = file_read_binary(cache_path, allocator);

        VkPipelineCacheHeaderVersionOne* cache_header = (VkPipelineCacheHeaderVersionOne*) read_result.data;

        if(cache_header->deviceID == vulkan_physical_device_properties.deviceID &&
           cache_header->vendorID == vulkan_physical_device_properties.vendorID &&
           memcmp(cache_header->pipelineCacheUUID, vulkan_physical_device_properties.pipelineCacheUUID, VK_UUID_SIZE) == 0)
        {
            pipeline_cache_create_info.initialDataSize = read_result.size;
            pipeline_cache_create_info.pInitialData = read_result.data;
        } else {
            cache_exists = false;
        }

        VkResult result = vkCreatePipelineCache(vulkan_device, &pipeline_cache_create_info, vulkan_allocation_callbacks, &pipeline_cache);
        check(result);

        allocator->deallocate(read_result.data);
    } else {
        VkResult result = vkCreatePipelineCache(vulkan_device, &pipeline_cache_create_info, vulkan_allocation_callbacks, &pipeline_cache);
        check(result);
    }

    ShaderStateHandle shader_state = create_shader_state(creation.shaders);
    if(shader_state.index == k_invalid_index) {
        // Shader did not compile
        pipelines.release_resource(handle.index);
        handle.index = k_invalid_index;

        return handle;
    }

    // Now that shaders are compiled, we can create the pipeline
    Pipeline* pipeline = access_pipeline(handle);
    ShaderState* shader_state_data = access_shader_state(shader_state);

    pipeline->shader_state = shader_state;

    VkDescriptorSetLayout vk_layouts[k_max_descriptor_set_layouts];

    u32 num_active_layouts = shader_state_data->parse_result->set_count;

    // Create VkPipelineLayout
    for(u32 l = 0; l < num_active_layouts; l++) {
        pipeline->descriptor_set_layout_handle[l] = create_descriptor_set_layout(shader_state_data->parse_result->sets[l]);
        pipeline->descriptor_set_layout[l] = access_descriptor_set_layout(pipeline->descriptor_set_layout_handle[l]);

        vk_layouts[l] = pipeline->descriptor_set_layout[l]->vk_descriptor_set_layout;
    }

    u32 bindless_active = 0;
    if(bindless_supported) {
        vk_layouts[num_active_layouts] = vulkan_bindless_descriptor_set_layout;
        bindless_active = 1;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    };
    pipeline_layout_info.pSetLayouts = vk_layouts;
    pipeline_layout_info.setLayoutCount = num_active_layouts + bindless_active;

    VkPipelineLayout pipeline_layout;
    VkResult result = vkCreatePipelineLayout(vulkan_device, &pipeline_layout_info, vulkan_allocation_callbacks, &pipeline_layout);
    check(result);

    // Cache pipeline layout
    pipeline->vk_pipeline_layout = pipeline_layout;
    pipeline->num_active_layouts = num_active_layouts;

    // Create full pipeline
    if(shader_state_data->graphics_pipeline) {
        VkGraphicsPipelineCreateInfo pipeline_info = {
                VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
        };

        // Shader stage
        pipeline_info.pStages = shader_state_data->shader_stage_info;
        pipeline_info.stageCount = shader_state_data->active_shaders;

        // Pipeline Layout
        pipeline_info.layout = pipeline_layout;

        // Vertex input
        VkPipelineVertexInputStateCreateInfo vertex_input_info = {
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
        };

        // Vertex attributes
        VkVertexInputAttributeDescription vertex_attributes[8];
        if(creation.vertex_input.num_vertex_attributes) {
            for(u32 i = 0; i < creation.vertex_input.num_vertex_attributes; i++) {
                const VertexAttribute& vertex_attribute = creation.vertex_input.vertex_attributes[i];
                vertex_attributes[i] = {
                        vertex_attribute.location,
                        vertex_attribute.binding,
                        to_vk_vertex_format(vertex_attribute.format),
                        vertex_attribute.offset
                };
            }

            vertex_input_info.vertexAttributeDescriptionCount = creation.vertex_input.num_vertex_attributes;
            vertex_input_info.pVertexAttributeDescriptions = vertex_attributes;
        } else {
            vertex_input_info.vertexAttributeDescriptionCount = 0;
            vertex_input_info.pVertexAttributeDescriptions = nullptr;
        }

        // Vertex bindings
        VkVertexInputBindingDescription vertex_bindings[8];
        if(creation.vertex_input.num_vertex_streams) {
            vertex_input_info.vertexBindingDescriptionCount = creation.vertex_input.num_vertex_streams;

            for(u32 i = 0; i < creation.vertex_input.num_vertex_streams; i++) {
                const VertexStream& vertex_stream = creation.vertex_input.vertex_streams[i];
                VkVertexInputRate vertex_rate = vertex_stream.input_rate == VertexInputRate::PerVertex ?
                        VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX : VkVertexInputRate::VK_VERTEX_INPUT_RATE_INSTANCE;
                vertex_bindings[i] = {
                        vertex_stream.binding,
                        vertex_stream.stride,
                        vertex_rate
                };
            }

            vertex_input_info.pVertexBindingDescriptions = vertex_bindings;
        } else {
            vertex_input_info.vertexBindingDescriptionCount = 0;
            vertex_input_info.pVertexBindingDescriptions = nullptr;
        }

        pipeline_info.pVertexInputState = &vertex_input_info;

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo input_assembly {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
        };
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        pipeline_info.pInputAssemblyState = &input_assembly;

        // Color blending
        VkPipelineColorBlendAttachmentState color_blend_attachment[8];
        if(creation.blend_state.active_states) {
            for(size_t i = 0; i < creation.blend_state.active_states; i++) {
                const BlendState& blend_state = creation.blend_state.blend_states[i];

                color_blend_attachment[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                color_blend_attachment[i].blendEnable = blend_state.blend_enabled ? VK_TRUE : VK_FALSE;
                color_blend_attachment[i].srcColorBlendFactor = blend_state.source_color;
                color_blend_attachment[i].dstColorBlendFactor = blend_state.destination_color;
                color_blend_attachment[i].colorBlendOp = blend_state.color_operation;

                if(blend_state.separate_blend) {
                    color_blend_attachment[i].srcAlphaBlendFactor = blend_state.source_alpha;
                    color_blend_attachment[i].dstAlphaBlendFactor = blend_state.destination_alpha;
                    color_blend_attachment[i].alphaBlendOp = blend_state.alpha_operation;
                } else {
                    color_blend_attachment[i].srcAlphaBlendFactor = blend_state.source_color;
                    color_blend_attachment[i].dstAlphaBlendFactor = blend_state.destination_color;
                    color_blend_attachment[i].alphaBlendOp = blend_state.color_operation;
                }
            }
        } else {
            // Default non-blended state
            color_blend_attachment[0] = {};
            color_blend_attachment[0].blendEnable = VK_FALSE;
            color_blend_attachment[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }

        VkPipelineColorBlendStateCreateInfo color_blending {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
        };
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.logicOp = VK_LOGIC_OP_COPY;
        color_blending.attachmentCount = creation.blend_state.active_states ? creation.blend_state.active_states : 1;
        color_blending.pAttachments = color_blend_attachment;
        color_blending.blendConstants[0] = 0.0f;
        color_blending.blendConstants[1] = 0.0f;
        color_blending.blendConstants[2] = 0.0f;
        color_blending.blendConstants[3] = 0.0f;

        pipeline_info.pColorBlendState = &color_blending;

        // Depth stencil
        VkPipelineDepthStencilStateCreateInfo depth_stencil = {
                VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
        };
        depth_stencil.depthWriteEnable = creation.depth_stencil.depth_write_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.stencilTestEnable = creation.depth_stencil.stencil_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.depthTestEnable = creation.depth_stencil.depth_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.depthCompareOp = creation.depth_stencil.depth_comparison;
        if(creation.depth_stencil.stencil_enable) {
            PASSERT(false);
        }

        pipeline_info.pDepthStencilState = &depth_stencil;

        // Multisample
        VkPipelineMultisampleStateCreateInfo multisampling = {
                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
        };
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        pipeline_info.pMultisampleState = &multisampling;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer = {
                VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
        };
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = creation.rasterization.cull_mode;
        rasterizer.frontFace = creation.rasterization.front;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        pipeline_info.pRasterizationState = &rasterizer;

        // Tessellation
        pipeline_info.pTessellationState;

        // Viewport State
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchain_width;
        viewport.height = (float)swapchain_height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = {swapchain_width, swapchain_height};

        VkPipelineViewportStateCreateInfo viewport_state = {
                VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO
        };
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        pipeline_info.pViewportState = &viewport_state;

        // Render Pass
        pipeline_info.renderPass = get_vulkan_render_pass(creation.render_pass, creation.name);

        // Dynamic States
        VkDynamicState dynamic_states[] = {
                VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamic_state = {
                VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO
        };
        dynamic_state.dynamicStateCount = PuffinArraySize(dynamic_states);
        dynamic_state.pDynamicStates = dynamic_states;

        pipeline_info.pDynamicState = &dynamic_state;

        vkCreateGraphicsPipelines(vulkan_device, pipeline_cache, 1, &pipeline_info, vulkan_allocation_callbacks, &pipeline->vk_pipeline);

        pipeline->vk_bind_point = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS;

    } else {
        VkComputePipelineCreateInfo pipeline_info { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

        pipeline_info.stage = shader_state_data->shader_stage_info[0];
        pipeline_info.layout = pipeline_layout;

        vkCreateComputePipelines(vulkan_device, pipeline_cache, 1, &pipeline_info, vulkan_allocation_callbacks, &pipeline->vk_pipeline);

        pipeline->vk_bind_point = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE;
    }

    if(cache_path != nullptr && !cache_exists) {
        size_t cache_data_size = 0 ;
        VkResult result = vkGetPipelineCacheData(vulkan_device, pipeline_cache, &cache_data_size, nullptr);
        check(result);

        void* cache_data = allocator->allocate(cache_data_size, 64);
        result = vkGetPipelineCacheData(vulkan_device, pipeline_cache, &cache_data_size, cache_data);
        check(result);

        file_write_binary(cache_path, cache_data, cache_data_size);

        allocator->deallocate(cache_data);
    }

    vkDestroyPipelineCache(vulkan_device, pipeline_cache, vulkan_allocation_callbacks);

    return handle;
}

BufferHandle GpuDevice::create_buffer(const BufferCreation& creation) {
    BufferHandle handle = { buffers.obtain_resource() };
    if(handle.index == k_invalid_index) {
        return handle;
    }

    Buffer* buffer = access_buffer(handle);
    buffer->name = creation.name;
    buffer->size = creation.size;
    buffer->type_flags = creation.type_flags;
    buffer->usage = creation.usage;
    buffer->handle = handle;
    buffer->global_offset = 0;
    buffer->parent_buffer = k_invalid_buffer;

    // Cache and calculate if dynamic buffer can be used
    static const VkBufferUsageFlags k_dynamic_buffer_mask = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    const bool use_global_buffer = (creation.type_flags & k_dynamic_buffer_mask) != 0;
    if(creation.usage == ResourceUsageType::Dynamic && use_global_buffer) {
        buffer->parent_buffer = dynamic_buffer;
        return handle;
    }

    VkBufferCreateInfo buffer_info = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO
    };
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | creation.type_flags;
    buffer_info.size = creation.size > 0 ? creation.size : 1; // 0 is not permitted

    VmaAllocationCreateInfo memory_info{};
    memory_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
    memory_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VmaAllocationInfo allocation_info{};
    VkResult result = vmaCreateBuffer(vma_allocator, &buffer_info, &memory_info, &buffer->vk_buffer, &buffer->vma_allocation,
                                      &allocation_info);
    check(result);

    set_resource_name(VK_OBJECT_TYPE_BUFFER, (u64)buffer->vk_buffer, creation.name);

    buffer->vk_device_memory = allocation_info.deviceMemory;

    if(creation.initial_data) {
        void* data;
        vmaMapMemory(vma_allocator, buffer->vma_allocation, &data);
        memcpy(data, creation.initial_data, (size_t)creation.size);
        vmaUnmapMemory(vma_allocator, buffer->vma_allocation);
    }

    return handle;
}

SamplerHandle GpuDevice::create_sampler(const SamplerCreation& creation) {
    SamplerHandle handle = { samplers.obtain_resource() };
    if(handle.index == k_invalid_index) {
        return handle;
    }

    Sampler* sampler = access_sampler(handle);
    sampler->address_mode_u = creation.address_mode_u;
    sampler->address_mode_v = creation.address_mode_v;
    sampler->address_mode_w = creation.address_mode_w;
    sampler->min_filter = creation.min_filter;
    sampler->mag_filter = creation.mag_filter;
    sampler->mip_filter = creation.mip_filter;
    sampler->name = creation.name;

    VkSamplerCreateInfo create_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    create_info.addressModeU = creation.address_mode_u;
    create_info.addressModeV = creation.address_mode_v;
    create_info.addressModeW = creation.address_mode_w;
    create_info.minFilter = creation.min_filter;
    create_info.magFilter = creation.mag_filter;
    create_info.mipmapMode = creation.mip_filter;
    create_info.anisotropyEnable =  0;
    create_info.compareEnable = 0;
    create_info.unnormalizedCoordinates = 0;
    create_info.borderColor = VkBorderColor::VK_BORDER_COLOR_INT_OPAQUE_WHITE;

    vkCreateSampler(vulkan_device, &create_info, vulkan_allocation_callbacks, &sampler->vk_sampler);

    set_resource_name(VK_OBJECT_TYPE_SAMPLER, (u64)sampler->vk_sampler, creation.name);

    return handle;
}

DescriptorSetLayoutHandle GpuDevice::create_descriptor_set_layout(const puffin::DescriptorSetLayoutCreation& creation) {
    DescriptorSetLayoutHandle handle = { descriptor_set_layouts.obtain_resource() };
    if(handle.index == k_invalid_index) {
        return handle;
    }

    DescriptorSetLayout* descriptor_set_layout = access_descriptor_set_layout(handle);

    descriptor_set_layout->num_bindings = (u16)creation.num_bindings;
    size_t memory_size = ( sizeof(VkDescriptorSetLayoutBinding) + sizeof(DescriptorBinding) ) * creation.num_bindings;
    u8* memory = puffin_alloc_return_mem_pointer(memory_size, allocator);
    descriptor_set_layout->bindings = (DescriptorBinding*) memory;
    descriptor_set_layout->vk_binding = (VkDescriptorSetLayoutBinding*)(memory + (sizeof(DescriptorBinding) * creation.num_bindings));
    descriptor_set_layout->handle = handle;
    descriptor_set_layout->set_index = u16(creation.set_index);

    u32 used_bindings = 0;
    for(u32 r = 0; r < creation.num_bindings; r++) {
        DescriptorBinding& binding = descriptor_set_layout->bindings[r];
        const DescriptorSetLayoutCreation::Binding& input_binding = creation.bindings[r];
        binding.start = input_binding.start == u16_max ? (u16)r : input_binding.start;
        binding.count = 1;
        binding.type = input_binding.type;
        binding.name = input_binding.name;

        if(bindless_supported && (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || binding.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)){
            continue;
        }

        VkDescriptorSetLayoutBinding& vk_binding = descriptor_set_layout->vk_binding[used_bindings];
        used_bindings++;

        vk_binding.binding = binding.start;
        vk_binding.descriptorType = input_binding.type;
        vk_binding.descriptorType = vk_binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : vk_binding.descriptorType;
        vk_binding.descriptorCount = 1;

        vk_binding.stageFlags = VK_SHADER_STAGE_ALL;
        vk_binding.pImmutableSamplers = nullptr;
    }

    // Create the descriptor set layout
    VkDescriptorSetLayoutCreateInfo layout_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
    };
    layout_info.bindingCount = used_bindings;
    layout_info.pBindings = descriptor_set_layout->vk_binding;

    vkCreateDescriptorSetLayout(vulkan_device, &layout_info, vulkan_allocation_callbacks, &descriptor_set_layout->vk_descriptor_set_layout);

    return handle;
}

void GpuDevice::fill_write_descriptor_sets(GpuDevice& gpu, const DescriptorSetLayout* descriptor_set_layout, VkDescriptorSet vk_descriptor_set,
                                       VkWriteDescriptorSet* descriptor_write, VkDescriptorBufferInfo* buffer_info, VkDescriptorImageInfo* image_info,
                                       VkSampler vk_default_sampler, u32& num_resources, const ResourceHandle* resources, const SamplerHandle* samplers, const u16* bindings) {

    u32 used_resources = 0;
    for(u32 r = 0; r < num_resources; r++) {
        // Binding array contains the index into the resource layout binding to retrieve
        // the correct binding information
        u32 layout_binding_index = bindings[r];

        const DescriptorBinding& binding = descriptor_set_layout->bindings[layout_binding_index];

        if(gpu.bindless_supported && (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || binding.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)) {
            continue;
        }

        u32 i = used_resources;
        used_resources++;

        descriptor_write[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptor_write[i].dstSet = vk_descriptor_set;
        // use binding array to get final binding point.
        const u32 binding_point = binding.start;
        descriptor_write[i].dstBinding = binding_point;
        descriptor_write[i].dstArrayElement = 0;
        descriptor_write[i].descriptorCount = 1;

        switch(binding.type) {
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            {
                descriptor_write[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

                TextureHandle texture_handle = { resources[r] };
                Texture* texture_data = gpu.access_texture(texture_handle);

                // Find proper sampler
                image_info[i].sampler = vk_default_sampler;
                if(texture_data->sampler) {
                    image_info[i].sampler = texture_data->sampler->vk_sampler;
                }

                if(samplers[r].index != k_invalid_index) {
                    Sampler* sampler = gpu.access_sampler({samplers[r]});
                    image_info[i].sampler = sampler->vk_sampler;
                }

                image_info[i].imageLayout = TextureFormat::has_depth_or_stencil(texture_data->vk_format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                image_info[i].imageView = texture_data->vk_image_view;

                descriptor_write[i].pImageInfo = &image_info[i];

                break;
            }

            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            {
                descriptor_write[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

                TextureHandle texture_handle = { resources[r] };
                Texture* texture_data = gpu.access_texture(texture_handle);

                image_info[i].sampler = nullptr;
                image_info[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                image_info[i].imageView = texture_data->vk_image_view;

                descriptor_write[i].pImageInfo = &image_info[i];

                break;
            }

            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            {
                BufferHandle buffer_handle = { resources[r] };
                Buffer* buffer = gpu.access_buffer(buffer_handle);

                descriptor_write[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptor_write[i].descriptorType = buffer->usage == ResourceUsageType::Dynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

                // Bind parent buffer if present, used for dynamic resources
                if(buffer->parent_buffer.index != k_invalid_index) {
                    Buffer* parent_buffer = gpu.access_buffer(buffer->parent_buffer);

                    buffer_info[i].buffer = parent_buffer->vk_buffer;
                } else {
                    buffer_info[i].buffer = buffer->vk_buffer;
                }

                buffer_info[i].offset = 0 ;
                buffer_info[i].range = buffer->size;

                descriptor_write[i].pBufferInfo = &buffer_info[i];

                break;
            }

            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            {
                BufferHandle buffer_handle = { resources[r] };
                Buffer* buffer = gpu.access_buffer(buffer_handle);

                descriptor_write[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

                // Bind parent buffer if present, used for dynamic resources
                if(buffer->parent_buffer.index != k_invalid_index) {
                    Buffer* parent_buffer = gpu.access_buffer(buffer->parent_buffer);

                    buffer_info[i].buffer = parent_buffer->vk_buffer;
                } else {
                    buffer_info[i].buffer = buffer->vk_buffer;
                }

                buffer_info[i].offset = 0;
                buffer_info[i].range = buffer->size;

                descriptor_write[i].pBufferInfo = &buffer_info[i];

                break;
            }

            default:
            {
                PASSERTM(false,"Resource type %d not supported in descriptor set creation!\n", binding.type);
                break;
            }
        }
    }

    num_resources = used_resources;

}

static void vulkan_fill_write_descriptor_sets(GpuDevice& gpu_device, const DescriptorSetLayout* descriptor_set_layout, VkDescriptorSet vk_descriptor_set,
                                              VkWriteDescriptorSet* descriptor_write, VkDescriptorBufferInfo* buffer_info, VkDescriptorImageInfo* image_info,
                                              VkSampler vk_default_sampler, u32& num_resources, const ResourceHandle* resources, const SamplerHandle* samplers, const u16* bindings) {
    u32 used_resources = 0;
    for(u32 r = 0; r < num_resources; r++) {
        // Binding array contains the index into the resource layout binding to retrieve
        // the correct binding information.
        u32 layout_binding_index = bindings[r];

        const DescriptorBinding& binding = descriptor_set_layout->bindings[layout_binding_index];

        if(gpu_device.bindless_supported && (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || binding.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)) {
            continue;
        }

        u32 i = used_resources;
        used_resources++;

        descriptor_write[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptor_write[i].dstSet = vk_descriptor_set;

        // Use binding array to get final binding point
        const u32 binding_point = binding.start;
        descriptor_write[i].dstBinding = binding_point;
        descriptor_write[i].dstArrayElement = 0;
        descriptor_write[i].descriptorCount = 1;

        switch(binding.type) {
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            {
                descriptor_write[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

                TextureHandle texture_handle = { resources[r] };
                Texture* texture_data = gpu_device.access_texture(texture_handle);

                // Find proper sampler
                image_info[i].sampler = vk_default_sampler;
                if(texture_data->sampler) {
                    image_info[i].sampler = texture_data->sampler->vk_sampler;
                }

                if(samplers[r].index != k_invalid_index) {
                    Sampler* sampler = gpu_device.access_sampler({samplers[r]});
                    image_info[i].sampler = sampler->vk_sampler;
                }

                image_info[i].imageLayout = TextureFormat::has_depth_or_stencil(texture_data->vk_format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                image_info[i].imageView = texture_data->vk_image_view;

                descriptor_write[i].pImageInfo = &image_info[i];

                break;
            }

            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            {
                descriptor_write[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

                TextureHandle texture_handle = { resources[r] };
                Texture* texture_data = gpu_device.access_texture(texture_handle);

                image_info[i].sampler = nullptr;
                image_info[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                image_info[i].imageView = texture_data->vk_image_view;

                descriptor_write[i].pImageInfo = &image_info[i];

                break;
            }

            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            {
                BufferHandle buffer_handle = { resources[r] };
                Buffer* buffer = gpu_device.access_buffer(buffer_handle);

                descriptor_write[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptor_write[i].descriptorType = buffer->usage == ResourceUsageType::Dynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

                // Bind the parent buffer if present, it is used with dynamic resources
                if(buffer->parent_buffer.index != k_invalid_index) {
                    Buffer* parent_buffer = gpu_device.access_buffer(buffer->parent_buffer);
                    buffer_info[i].buffer = parent_buffer->vk_buffer;
                } else {
                    buffer_info[i].buffer = buffer->vk_buffer;
                }

                buffer_info[i].offset = 0;
                buffer_info[i].range = buffer->size;

                descriptor_write[i].pBufferInfo = &buffer_info[i];

                break;
            }

            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            {
                BufferHandle buffer_handle = { resources[r] };
                Buffer* buffer = gpu_device.access_buffer(buffer_handle);

                descriptor_write[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                // Bind parent buffer if present, used for dynamic resources.
                if(buffer->parent_buffer.index != k_invalid_index) {
                    Buffer* parent_buffer = gpu_device.access_buffer(buffer->parent_buffer);

                    buffer_info[i].buffer = parent_buffer->vk_buffer;
                } else {
                    buffer_info[i].buffer = buffer->vk_buffer;
                }

                buffer_info[i].offset = 0;
                buffer_info[i].range = buffer->size;

                descriptor_write[i].pBufferInfo = &buffer_info[i];

                break;
            }

            default:
            {
                PASSERTM(false, "Resource type %d not supported in descriptor set creation!\n", binding.type);
                break;
            }
        }
    }

    num_resources = used_resources;
}


DescriptorSetHandle GpuDevice::create_descriptor_set(const DescriptorSetCreation& creation) {
    DescriptorSetHandle handle = { descriptor_sets.obtain_resource() };
    if(handle.index == k_invalid_index) {
        return handle;
    }

    DescriptorSet* descriptor_set = access_descriptor_set(handle);
    const DescriptorSetLayout* descriptor_set_layout = access_descriptor_set_layout(creation.layout);

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    alloc_info.descriptorPool = vulkan_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout->vk_descriptor_set_layout;

    VkResult result = vkAllocateDescriptorSets(vulkan_device, &alloc_info, &descriptor_set->vk_descriptor_set);
    check(result);

    // Cache data
    u8* memory = puffin_alloc_return_mem_pointer((sizeof(ResourceHandle) + sizeof(SamplerHandle) + sizeof(u16)) * creation.num_resources, allocator);
    descriptor_set->resources = (ResourceHandle*) memory;
    descriptor_set->samplers = (SamplerHandle*) (memory + sizeof(ResourceHandle) * creation.num_resources);
    descriptor_set->bindings = (u16*) (memory + (sizeof(ResourceHandle) + sizeof(SamplerHandle)) * creation.num_resources);
    descriptor_set->num_resources = creation.num_resources;
    descriptor_set->layout = descriptor_set_layout;

    // Update descriptor set
    VkWriteDescriptorSet descriptor_write[8];
    VkDescriptorBufferInfo buffer_info[8];
    VkDescriptorImageInfo image_info[8];

    Sampler* vk_default_sampler = access_sampler(default_sampler);

    u32 num_resources = creation.num_resources;
    vulkan_fill_write_descriptor_sets(*this, descriptor_set_layout, descriptor_set->vk_descriptor_set, descriptor_write,
                                      buffer_info, image_info, vk_default_sampler->vk_sampler, num_resources,
                                      creation.resources, creation.samplers, creation.bindings);

    // Cache resources
    for(u32 r = 0; r < creation.num_resources; r++) {
        descriptor_set->resources[r] = creation.resources[r];
        descriptor_set->samplers[r] = creation.samplers[r];
        descriptor_set->bindings[r] = creation.bindings[r];
    }

    vkUpdateDescriptorSets(vulkan_device, num_resources, descriptor_write, 0, nullptr);

    return handle;
}

static void vulkan_create_swapchain_pass(GpuDevice& gpu, const RenderPassCreation& creation, RenderPass* render_pass) {
    // Color attachment
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = gpu.vulkan_surface_format.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment
    VkAttachmentDescription depth_attachment = {};
    Texture* depth_texture_vk = gpu.access_texture(gpu.depth_texture);
    depth_attachment.format = depth_texture_vk->vk_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    VkAttachmentDescription attachments[] = { color_attachment, depth_attachment };
    VkRenderPassCreateInfo render_pass_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VkResult result = vkCreateRenderPass(gpu.vulkan_device, &render_pass_info, nullptr, &render_pass->vk_render_pass);
    check(result);

    gpu.set_resource_name(VK_OBJECT_TYPE_RENDER_PASS, (u64)render_pass->vk_render_pass, creation.name);

    // Create framebuffer into the device
    VkFramebufferCreateInfo framebuffer_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebuffer_info.renderPass = render_pass->vk_render_pass;
    framebuffer_info.attachmentCount = 2;
    framebuffer_info.width = gpu.swapchain_width;
    framebuffer_info.height = gpu.swapchain_height;
    framebuffer_info.layers = 1;

    VkImageView framebuffer_attachments[2];
    framebuffer_attachments[1] = depth_texture_vk->vk_image_view;

    for(size_t i = 0; i < gpu.vulkan_swapchain_image_count; i++) {
        framebuffer_attachments[0] = gpu.vulkan_swapchain_image_views[i];
        framebuffer_info.pAttachments = framebuffer_attachments;

        result = vkCreateFramebuffer(gpu.vulkan_device, &framebuffer_info, nullptr, &gpu.vulkan_swapchain_framebuffers[i]);
        check(result);
        gpu.set_resource_name(VK_OBJECT_TYPE_FRAMEBUFFER, (u64)gpu.vulkan_swapchain_framebuffers[i], creation.name);
    }

    render_pass->width = gpu.swapchain_width;
    render_pass->height = gpu.swapchain_height;

    // Manually transition the texture
    VkCommandBufferBeginInfo begin_info = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    CommandBuffer* command_buffer = gpu.get_instant_command_buffer();
    vkBeginCommandBuffer(command_buffer->vk_command_buffer, &begin_info);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0,0,0};
    region.imageExtent = {gpu.swapchain_width, gpu.swapchain_height};

    // Transition
    for(size_t i = 0; i < gpu.vulkan_swapchain_image_count; i++) {
        transition_image_layout(command_buffer->vk_command_buffer, gpu.vulkan_swapchain_images[i], gpu.vulkan_surface_format.format,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, false);
    }
    transition_image_layout(command_buffer->vk_command_buffer, depth_texture_vk->vk_image, depth_texture_vk->vk_format,
                            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, true);

    vkEndCommandBuffer(command_buffer->vk_command_buffer);

    // Submit command buffer
    VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer->vk_command_buffer;

    vkQueueSubmit(gpu.vulkan_queue, 1, &submit_info, VK_NULL_HANDLE);
    // vkQueueWaitIdle is equivalent to having submitted a valid fence to every
    // previously executed queue submission command that accepts a fence, then waiting for all of
    // those fences to signal using vkWaitForFences with an infinite timeout and waitAll set to VK_TRUE.
    vkQueueWaitIdle(gpu.vulkan_queue);
}

static void vulkan_create_framebuffer(GpuDevice& gpu, RenderPass* render_pass, const TextureHandle* output_textures, u32 num_render_targets, TextureHandle depth_stencil_texture) {
    VkFramebufferCreateInfo framebuffer_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebuffer_info.renderPass = render_pass->vk_render_pass;
    framebuffer_info.width = render_pass->width;
    framebuffer_info.height = render_pass->height;
    framebuffer_info.layers = 1;

    VkImageView framebuffer_attachments[k_max_image_outputs + 1] = {};
    u32 active_attachments = 0;
    for( ; active_attachments < num_render_targets; active_attachments++) {
        Texture* texture_vk = gpu.access_texture(output_textures[active_attachments]);
        framebuffer_attachments[active_attachments] = texture_vk->vk_image_view;
    }

    if(depth_stencil_texture.index != k_invalid_index) {
        Texture* depth_texture_vk = gpu.access_texture(depth_stencil_texture);
        framebuffer_attachments[active_attachments++] = depth_texture_vk->vk_image_view;
    }

    framebuffer_info.pAttachments = framebuffer_attachments;
    framebuffer_info.attachmentCount = active_attachments;

    vkCreateFramebuffer(gpu.vulkan_device, & framebuffer_info, nullptr, &render_pass->vk_framebuffer);
    gpu.set_resource_name(VK_OBJECT_TYPE_FRAMEBUFFER, (u64)render_pass->vk_framebuffer, render_pass->name);
}

static VkRenderPass vulkan_create_render_pass(GpuDevice& gpu, const RenderPassOutput& output, cstring name) {
    VkAttachmentDescription color_attachments[8] = {};
    VkAttachmentReference color_attachments_refs[8] = {};

    VkAttachmentLoadOp color_op, depth_op, stencil_op;
    VkImageLayout color_initial, depth_initial;

    switch(output.color_operation) {
        case RenderPassOperation::Load:
            color_op = VK_ATTACHMENT_LOAD_OP_LOAD;
            color_initial = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case RenderPassOperation::Clear:
            color_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_initial = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        default:
            color_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color_initial = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
    }

    switch(output.depth_operation) {
        case RenderPassOperation::Load:
            depth_op = VK_ATTACHMENT_LOAD_OP_LOAD;
            depth_initial = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            break;
        case RenderPassOperation::Clear:
            depth_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_initial = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            break;
        default:
            depth_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depth_initial = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
    }

    switch(output.stencil_operation) {
        case RenderPassOperation::Load:
            stencil_op = VK_ATTACHMENT_LOAD_OP_LOAD;
            break;
        case RenderPassOperation::Clear:
            stencil_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
            break;
        default:
            stencil_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            break;
    }

    // Color attachments
    u32 c = 0;
    for(; c < output.num_color_formats; c++) {
        VkAttachmentDescription& color_attachment = color_attachments[c];
        color_attachment.format = output.color_formats[c];
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = color_op;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = stencil_op;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = color_initial;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference& color_attachment_ref = color_attachments_refs[c];
        color_attachment_ref.attachment = c;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    // Depth attachment
    VkAttachmentDescription depth_attachment = {};
    VkAttachmentReference depth_attachment_ref = {};

    if(output.depth_stencil_format != VK_FORMAT_UNDEFINED) {
        depth_attachment.format = output.depth_stencil_format;
        depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment.loadOp = depth_op;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment.stencilLoadOp = stencil_op;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = depth_initial;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depth_attachment_ref.attachment = c;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    // Create subpass
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Calculate active attachments for the subpass
    VkAttachmentDescription attachments[k_max_image_outputs + 1] = {};

    u32 active_attachments = 0;
    for(; active_attachments < output.num_color_formats; active_attachments++) {
        attachments[active_attachments] = color_attachments[active_attachments];
        active_attachments++; // TODO!! Find out if this second increment is on purpose
    }
    subpass.colorAttachmentCount = active_attachments ? active_attachments - 1 : 0;
    subpass.pColorAttachments = color_attachments_refs;

    subpass.pDepthStencilAttachment = nullptr;

    u32 depth_stencil_count = 0;
    if(output.depth_stencil_format != VK_FORMAT_UNDEFINED) {
        attachments[subpass.colorAttachmentCount] = depth_attachment;
        subpass.pDepthStencilAttachment = &depth_attachment_ref;
        depth_stencil_count = 1;
    }

    VkRenderPassCreateInfo render_pass_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    render_pass_info.attachmentCount = subpass.colorAttachmentCount + depth_stencil_count;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VkRenderPass vk_render_pass;
    VkResult result = vkCreateRenderPass(gpu.vulkan_device, &render_pass_info, nullptr, &vk_render_pass);
    check(result);

    gpu.set_resource_name(VK_OBJECT_TYPE_RENDER_PASS, (u64)vk_render_pass, name);

    return vk_render_pass;
}

static RenderPassOutput fill_render_pass_output(GpuDevice& gpu, const RenderPassCreation& creation) {
    RenderPassOutput output;
    output.reset();

    for(u32 i = 0; i < creation.num_render_targets; i++) {
        Texture* texture_vk = gpu.access_texture(creation.output_textures[i]);
        output.color(texture_vk->vk_format);
    }

    if(creation.depth_stencil_texture.index != k_invalid_index) {
        Texture* texture_vk = gpu.access_texture(creation.depth_stencil_texture);
        output.depth(texture_vk->vk_format);
    }

    output.color_operation = creation.color_operation;
    output.depth_operation = creation.depth_operation;
    output.stencil_operation = creation.stencil_operation;

    return output;
}

RenderPassHandle GpuDevice::create_render_pass(const RenderPassCreation& creation) {
    RenderPassHandle handle = { render_passes.obtain_resource() };
    if(handle.index == k_invalid_index) {
        return handle;
    }

    RenderPass* render_pass = access_render_pass(handle);
    render_pass->type = creation.type;
    render_pass->num_render_targets = (u8)creation.num_render_targets;
    render_pass->dispatch_x = 0;
    render_pass->dispatch_y = 0;
    render_pass->dispatch_z = 0;
    render_pass->name = creation.name;
    render_pass->vk_framebuffer = nullptr;
    render_pass->vk_render_pass = nullptr;
    render_pass->scale_x = creation.scale_x;
    render_pass->scale_y = creation.scale_y;
    render_pass->resize = creation.resize;

    // Cache render handles
    u32 c = 0;
    for( ; c < creation.num_render_targets; c++) {
        Texture* texture_vk = access_texture(creation.output_textures[c]);

        render_pass->width = texture_vk->width;
        render_pass->height = texture_vk->height;

        // Cache texture handles
        render_pass->output_textures[c] = creation.output_textures[c];
    }

    switch(creation.type) {
        case RenderPassType::Swapchain:
        {
            vulkan_create_swapchain_pass(*this, creation, render_pass);
            break;
        }

        case RenderPassType::Compute:
        {
            break;
        }

        case RenderPassType::Geometry:
        {
            render_pass->output = fill_render_pass_output(*this, creation);
            render_pass->vk_render_pass = get_vulkan_render_pass(render_pass->output, creation.name);

            vulkan_create_framebuffer(*this, render_pass, creation.output_textures, creation.num_render_targets, creation.depth_stencil_texture);
            break;
        }
    }

    return handle;
}

// Resource destruction //////
void GpuDevice::destroy_buffer(BufferHandle buffer) {
    if(buffer.index < buffers.pool_size) {
        resource_deletion_queue.push({
            ResourceDeletionType::Buffer,
            buffer.index,
            current_frame
        });
    } else {
        p_print("Graphics error: trying to free invalid Buffer %u\n", buffer.index);
    }
}

void GpuDevice::destroy_texture(TextureHandle texture) {
    if(texture.index < textures.pool_size) {
        resource_deletion_queue.push({
                                             ResourceDeletionType::Texture,
                                             texture.index,
                                             current_frame
                                     });
    } else {
        p_print("Graphics error: trying to free invalid texture %u\n", texture.index);
    }
}

void GpuDevice::destroy_pipeline(PipelineHandle pipeline) {
    if(pipeline.index < pipelines.pool_size) {
        resource_deletion_queue.push({
                                             ResourceDeletionType::Pipeline,
                                             pipeline.index,
                                             current_frame
                                     });
        Pipeline* vk_pipeline = access_pipeline(pipeline);
        destroy_shader_state(vk_pipeline->shader_state);
    } else {
        p_print("Graphics error: trying to free invalid pipeline %u\n", pipeline.index);
    }
}

void GpuDevice::destroy_sampler(SamplerHandle sampler) {
    if(sampler.index < samplers.pool_size) {
        resource_deletion_queue.push({
                                             ResourceDeletionType::Sampler,
                                             sampler.index,
                                             current_frame
                                     });
    } else {
        p_print("Graphics error: trying to free invalid sampler %u\n", sampler.index);
    }
}

void GpuDevice::destroy_descriptor_set_layout(DescriptorSetLayoutHandle descriptor_set_layout) {
    if(descriptor_set_layout.index < descriptor_set_layouts.pool_size) {
        resource_deletion_queue.push({
                                             ResourceDeletionType::DescriptorSetLayout,
                                             descriptor_set_layout.index,
                                             current_frame
                                     });
    } else {
        p_print("Graphics error: trying to free invalid descriptor set layout %u\n", descriptor_set_layout.index);
    }
}

void GpuDevice::destroy_descriptor_set(DescriptorSetHandle descriptor_set) {
    if(descriptor_set.index < descriptor_sets.pool_size) {
        resource_deletion_queue.push({
                                             ResourceDeletionType::DescriptorSet,
                                             descriptor_set.index,
                                             current_frame
                                     });
    } else {
        p_print("Graphics error: trying to free invalid descriptor set %u\n", descriptor_set.index);
    }
}

void GpuDevice::destroy_render_pass(RenderPassHandle render_pass) {
    if(render_pass.index < render_passes.pool_size) {
        resource_deletion_queue.push({
                                             ResourceDeletionType::RenderPass,
                                             render_pass.index,
                                             current_frame
                                     });
    } else {
        p_print("Graphics error: trying to free invalid render_pass %u\n", render_pass.index);
    }
}

void GpuDevice::destroy_shader_state(ShaderStateHandle shader_state) {
    if(shader_state.index < shaders.pool_size) {
        resource_deletion_queue.push({
                                             ResourceDeletionType::ShaderState,
                                             shader_state.index,
                                             current_frame
                                     });
    } else {
        p_print("Graphics error: trying to free invalid shader state %u\n", shader_state.index);
    }
}

// Real destruction methods - not simply enqueuing
void GpuDevice::destroy_buffer_instant(ResourceHandle buffer_handle) {
    Buffer* buffer = (Buffer*)buffers.access_resource(buffer_handle);

    if(buffer && buffer->parent_buffer.index == k_invalid_buffer.index) {
        vmaDestroyBuffer(vma_allocator, buffer->vk_buffer, buffer->vma_allocation);
    }
    buffers.release_resource(buffer_handle);
}

void GpuDevice::destroy_texture_instant(ResourceHandle texture_handle) {
    Texture* texture = (Texture*)textures.access_resource(texture_handle);

    if(texture) {
        vkDestroyImageView(vulkan_device, texture->vk_image_view, vulkan_allocation_callbacks);
        vmaDestroyImage(vma_allocator, texture->vk_image, texture->vma_allocation);
    }
    textures.release_resource(texture_handle);
}

void GpuDevice::destroy_pipeline_instant(ResourceHandle pipeline_handle) {
    Pipeline* pipeline = (Pipeline*)pipelines.access_resource(pipeline_handle);

    if(pipeline) {
        vkDestroyPipeline(vulkan_device, pipeline->vk_pipeline, vulkan_allocation_callbacks);
        vkDestroyPipelineLayout(vulkan_device, pipeline->vk_pipeline_layout, vulkan_allocation_callbacks);
    }
    pipelines.release_resource(pipeline_handle);
}

void GpuDevice::destroy_sampler_instant(ResourceHandle sampler_handle) {
    Sampler* sampler = (Sampler*)samplers.access_resource(sampler_handle);

    if(sampler) {
        vkDestroySampler(vulkan_device, sampler->vk_sampler, vulkan_allocation_callbacks);
    }
    samplers.release_resource(sampler_handle);
}

void GpuDevice::destroy_descriptor_set_layout_instant(ResourceHandle layout_handle) {
    DescriptorSetLayout* descriptor_set_layout = (DescriptorSetLayout*)descriptor_set_layouts.access_resource(layout_handle);

    if(descriptor_set_layout) {
        vkDestroyDescriptorSetLayout(vulkan_device, descriptor_set_layout->vk_descriptor_set_layout, vulkan_allocation_callbacks);

        puffin_free(descriptor_set_layout->bindings, allocator);
    }
    descriptor_set_layouts.release_resource(layout_handle);
}

void GpuDevice::destroy_descriptor_set_instant(ResourceHandle descriptor_set_handle) {
    DescriptorSet* descriptor_set = (DescriptorSet*) descriptor_sets.access_resource(descriptor_set_handle);

    if(descriptor_set) {
        puffin_free(descriptor_set->resources, allocator);
        // the set is free along with its pool
    }
    descriptor_sets.release_resource(descriptor_set_handle);
}

void GpuDevice::destroy_render_pass_instant(ResourceHandle render_pass_handle) {
    RenderPass* render_pass = (RenderPass*) render_passes.access_resource(render_pass_handle);

    if(render_pass) {
        if(render_pass->num_render_targets) {
            vkDestroyFramebuffer(vulkan_device, render_pass->vk_framebuffer, vulkan_allocation_callbacks);
        }
        // render pass is destroyed with the render pass cache
    }
}

void GpuDevice::destroy_shader_state_instant(ResourceHandle shader_handle) {
    ShaderState* shader_state = (ShaderState*)shaders.access_resource(shader_handle);

    if(shader_state) {
        for(size_t i = 0; i < shader_state->active_shaders; i++) {
            vkDestroyShaderModule(vulkan_device, shader_state->shader_stage_info[i].module, vulkan_allocation_callbacks);
        }
    }
    shaders.release_resource(shader_handle);
}

void GpuDevice::set_resource_name(VkObjectType object_type, uint64_t handle, cstring name) {
    if(!debug_utils_extension_present) {
        return;
    }

    VkDebugUtilsObjectNameInfoEXT name_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    name_info.objectType = object_type;
    name_info.objectHandle = handle;
    name_info.pObjectName = name;
    pfnSetDebugUtilsObjectNameEXT(vulkan_device, &name_info);
}

void GpuDevice::push_marker(VkCommandBuffer command_buffer, cstring name) {
    VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    label.pLabelName = name;
    label.color[0] = 1.0f;
    label.color[1] = 1.0f;
    label.color[2] = 1.0f;
    label.color[3] = 1.0f;
    pfnCmdBeginDebugUtilsLabelEXT(command_buffer, &label);
}

void GpuDevice::pop_marker(VkCommandBuffer command_buffer) {
    pfnCmdEndDebugUtilsLabelEXT(command_buffer);
}

// Swapchain /////////////////////////

template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
    PASSERT(!(hi < lo));
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

void GpuDevice::create_swapchain() {
    // Check if surface is supported
    VkBool32 surface_supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(vulkan_physical_device, vulkan_queue_family, vulkan_window_surface, &surface_supported);
    if(!surface_supported) {
        p_print("Error! NO WSI support on physical device 0\n");
    }

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan_physical_device, vulkan_window_surface, &surface_capabilities);

    VkExtent2D swapchain_extent = surface_capabilities.currentExtent;
    if(swapchain_extent.width = UINT32_MAX) {
        swapchain_extent.width = clamp(swapchain_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
        swapchain_extent.height = clamp(swapchain_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    }

    p_print("Create swapchain %u %u - saved %u %u, min image %u\n", swapchain_extent.width, swapchain_extent.height, swapchain_width, swapchain_height, surface_capabilities.minImageCount);

    swapchain_width = (u16)swapchain_extent.width;
    swapchain_height = (u16)swapchain_extent.height;

    VkSwapchainCreateInfoKHR swapchain_create_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchain_create_info.surface = vulkan_window_surface;
    swapchain_create_info.minImageCount = vulkan_swapchain_image_count;
    swapchain_create_info.imageFormat = vulkan_surface_format.format;
    swapchain_create_info.imageExtent = swapchain_extent;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.preTransform = surface_capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = vulkan_present_mode;

    VkResult result = vkCreateSwapchainKHR(vulkan_device, &swapchain_create_info, 0, &vulkan_swapchain);
    check(result);

    // Cache swapchain images
    result = vkGetSwapchainImagesKHR(vulkan_device, vulkan_swapchain, &vulkan_swapchain_image_count, NULL);
    check(result);
    result = vkGetSwapchainImagesKHR(vulkan_device, vulkan_swapchain, &vulkan_swapchain_image_count, vulkan_swapchain_images);
    check(result);

    for(size_t iv = 0; iv < vulkan_swapchain_image_count; iv++) {
        // Create an image view which we can render to
        VkImageViewCreateInfo view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = vulkan_surface_format.format;
        view_info.image = vulkan_swapchain_images[iv];
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.components.r = VK_COMPONENT_SWIZZLE_R;
        view_info.components.g = VK_COMPONENT_SWIZZLE_G;
        view_info.components.b = VK_COMPONENT_SWIZZLE_B;
        view_info.components.a = VK_COMPONENT_SWIZZLE_A;

        result = vkCreateImageView(vulkan_device, &view_info, vulkan_allocation_callbacks, &vulkan_swapchain_image_views[iv]);
        check(result);
    }
}

void GpuDevice::destroy_swapchain() {
    for(size_t iv = 0; iv < vulkan_swapchain_image_count; iv++) {
        vkDestroyImageView(vulkan_device, vulkan_swapchain_image_views[iv], vulkan_allocation_callbacks);
        vkDestroyFramebuffer(vulkan_device, vulkan_swapchain_framebuffers[iv], vulkan_allocation_callbacks);
    }
    vkDestroySwapchainKHR(vulkan_device, vulkan_swapchain, vulkan_allocation_callbacks);
}

VkRenderPass GpuDevice::get_vulkan_render_pass(const RenderPassOutput& output, cstring name) {
    // Hash the memory output and find the referenced VkRenderPass
    // RenderPassOutput should save everything we need
    u64 hashed_memory = puffin::hash_bytes((void*)&output, sizeof(RenderPassOutput));
    VkRenderPass vulkan_render_pass = render_pass_cache.get(hashed_memory);
    if(vulkan_render_pass) {
        return vulkan_render_pass;
    }

    vulkan_render_pass = vulkan_create_render_pass(*this, output, name);
    render_pass_cache.insert(hashed_memory, vulkan_render_pass);
    return vulkan_render_pass;
}

static void vulkan_resize_texture(GpuDevice& gpu, Texture* v_texture, Texture* v_texture_to_delete, u16 width, u16 height, u16 depth) {
    // Cache handles to destroy after function, since we're updating texture in place
    v_texture_to_delete->vk_image_view = v_texture->vk_image_view;
    v_texture_to_delete->vk_image = v_texture->vk_image;
    v_texture_to_delete->vma_allocation = v_texture->vma_allocation;

    // Re-create image in place
    TextureCreation tc;
    tc.set_flags(v_texture->mipmaps, v_texture->flags)
        .set_format_type(v_texture->vk_format, v_texture->type)
        .set_name(v_texture->name)
        .set_size(width, height, depth);
    vulkan_create_texture(gpu, tc, v_texture->handle, v_texture);
}

void GpuDevice::resize_swapchain() {
    vkDeviceWaitIdle(vulkan_device);

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan_physical_device, vulkan_window_surface, &surface_capabilities);
    VkExtent2D swapchain_extent = surface_capabilities.currentExtent;

    // Skip zero-sized swapchain
    if(swapchain_extent.width == 0 || swapchain_extent.height) {
        return;
    }

    // Internal destroy of swapchain pass to reatin the same handle
    RenderPass* vk_swapchain_pass = access_render_pass(swapchain_pass);
    vkDestroyRenderPass(vulkan_device, vk_swapchain_pass->vk_render_pass, vulkan_allocation_callbacks);

    // Destroy swapchain images and framebuffers
    destroy_swapchain();
    vkDestroySurfaceKHR(vulkan_instance, vulkan_window_surface, vulkan_allocation_callbacks);

    VkResult result = glfwCreateWindowSurface(vulkan_instance, glfw_window, NULL, &vulkan_window_surface);
    check(result);

    create_swapchain();

    // Resize depth texture, maintaining handle, using a dummy texture to destroy
    TextureHandle texture_to_delete = { textures.obtain_resource() };
    Texture* vk_texture_to_delete = access_texture(texture_to_delete);
    vk_texture_to_delete->handle = texture_to_delete;
    Texture* vk_depth_texture = access_texture(depth_texture);
    vulkan_resize_texture(*this, vk_depth_texture, vk_texture_to_delete, swapchain_width, swapchain_height, 1);

    destroy_texture(texture_to_delete);

    RenderPassCreation swapchain_pass_creation = {};
    swapchain_pass_creation.set_type(RenderPassType::Swapchain)
        .set_name("Swapchain");
    vulkan_create_swapchain_pass(*this, swapchain_pass_creation, vk_swapchain_pass);

    vkDeviceWaitIdle(vulkan_device);
}

// Descriptor Set ///////////////////////////
void GpuDevice::update_descriptor_set(DescriptorSetHandle descriptor_set) {
    if(descriptor_set.index < descriptor_sets.pool_size) {
        DescriptorSetUpdate new_update = { descriptor_set, current_frame };
        descriptor_set_updates.push(new_update);
    } else {
        p_print("Graphics error: trying to update invalid DescriptorSet &u\n", descriptor_set.index);
    }
}

void GpuDevice::update_descriptor_set_instant(const DescriptorSetUpdate& update) {
    // Use a dummy descriptor set to delete the vulkan descriptor set handle
    DescriptorSetHandle dummy_delete_descriptor_set_handle = { descriptor_sets.obtain_resource() };
    DescriptorSet* dummy_delete_descriptor_set = access_descriptor_set(dummy_delete_descriptor_set_handle);

    DescriptorSet* descriptor_set = access_descriptor_set(update.descriptor_set);
    const DescriptorSetLayout* descriptor_set_layout = descriptor_set->layout;

    dummy_delete_descriptor_set->vk_descriptor_set = descriptor_set->vk_descriptor_set;
    dummy_delete_descriptor_set->bindings = nullptr;
    dummy_delete_descriptor_set->resources = nullptr;
    dummy_delete_descriptor_set->samplers = nullptr;
    dummy_delete_descriptor_set->num_resources = 0;

    destroy_descriptor_set(dummy_delete_descriptor_set_handle);

    // Allocate the new descriptor set and update its content
    VkWriteDescriptorSet descriptor_write[8];
    VkDescriptorBufferInfo buffer_info[8];
    VkDescriptorImageInfo image_info[8];

    Sampler* vk_default_sampler = access_sampler(default_sampler);

    VkDescriptorSetAllocateInfo allocInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = vulkan_descriptor_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptor_set->layout->vk_descriptor_set_layout;
    vkAllocateDescriptorSets(vulkan_device, &allocInfo, &descriptor_set->vk_descriptor_set);

    u32 num_resources = descriptor_set_layout->num_bindings;
    vulkan_fill_write_descriptor_sets(*this, descriptor_set_layout, descriptor_set->vk_descriptor_set,
                                        descriptor_write, buffer_info, image_info, vk_default_sampler->vk_sampler,
                                        num_resources, descriptor_set->resources, descriptor_set->samplers,
                                        descriptor_set->bindings);

    vkUpdateDescriptorSets(vulkan_device, num_resources, descriptor_write, 0, nullptr);
}

void GpuDevice::resize_output_textures(puffin::RenderPassHandle render_pass, u32 width, u32 height) {

    // For each texture, create a temporary pooled texture and cache the handles to delete.
    // This is because we substitute just the Vulkan texture when resizing so that
    // external users don't need to update the handle.

    RenderPass* vk_render_pass = access_render_pass(render_pass);
    if(vk_render_pass) {
        // no need to resize
        if(!vk_render_pass->resize) {
            return;
        }

        // Calculate new width and height based on render pass sizing information
        u16 new_width = (u16)(width * vk_render_pass->scale_x);
        u16 new_height = (u16)(height * vk_render_pass->scale_y);

        // Resize textures if needed
        const u32 rts = vk_render_pass->num_render_targets;
        for(u32 i = 0; i < rts; i++) {
            TextureHandle texture = vk_render_pass->output_textures[i];
            Texture* vk_texture = access_texture(texture);

            if(vk_texture->width == new_width && vk_texture->height == new_height) {
                continue;
            }

            // Queue deletion of texture by creating a temporary one
            TextureHandle texture_to_delete = { textures.obtain_resource() };
            Texture* vk_texture_to_delete = access_texture(texture_to_delete);

            // Update handle so it can be used to update bindless to dummy texture
            vk_texture_to_delete->handle = texture_to_delete;
            vulkan_resize_texture(*this, vk_texture, vk_texture_to_delete, new_width, new_height, 1);

            destroy_texture(texture_to_delete);
        }

        if(vk_render_pass->output_depth.index != k_invalid_index) {
            Texture* vk_texture = access_texture(vk_render_pass->output_depth);

            if(vk_texture->width != new_width || vk_texture->height != new_height) {
                // Queue deletion of texture by creating a temporary one
                TextureHandle texture_to_delete = { textures.obtain_resource() };
                Texture* vk_texture_to_delete = access_texture(texture_to_delete);
                // Update the handle so it can be used to update bindless to dummy texture
                vk_texture_to_delete->handle = texture_to_delete;
                vulkan_resize_texture(*this, vk_texture, vk_texture_to_delete, new_width, new_height, 1);

                destroy_texture(texture_to_delete);
            }
        }

        // Again: create temporary resources to use the standard deferred deletion mechanism
        RenderPassHandle render_pass_to_destroy = { render_passes.obtain_resource() };
        RenderPass* vk_render_pass_to_destroy = access_render_pass(render_pass_to_destroy);

        vk_render_pass_to_destroy->vk_framebuffer = vk_render_pass->vk_framebuffer;
        // This is checked in the destroy method to proceed with frame buffer destruction
        vk_render_pass_to_destroy->num_render_targets = 1;
        // Set to 0 so deletion won't be performed
        vk_render_pass_to_destroy->vk_render_pass = 0;

        destroy_render_pass(render_pass_to_destroy);

        // Update render pass size
        vk_render_pass->width = new_width;
        vk_render_pass->height = new_height;

        // Recreate the framebuffer if present (mainly for dispatch-only passes)
        if(vk_render_pass->vk_framebuffer) {
            vulkan_create_framebuffer(*this, vk_render_pass, vk_render_pass->output_textures,
                                      vk_render_pass->num_render_targets, vk_render_pass->output_depth);
        }
    }
}

void GpuDevice::fill_barrier(RenderPassHandle render_pass, ExecutionBarrier& out_barrier) {
    RenderPass* vk_render_pass = access_render_pass(render_pass);

    out_barrier.num_image_barriers = 0;

    if(vk_render_pass) {
        const u32 rts = vk_render_pass->num_render_targets;
        for(u32 i = 0; i < rts; i++) {
            out_barrier.image_barriers[out_barrier.num_image_barriers++].texture = vk_render_pass->output_textures[i];
        }

        if(vk_render_pass->output_depth.index != k_invalid_index) {
            out_barrier.image_barriers[out_barrier.num_image_barriers++].texture = vk_render_pass->output_depth;
        }
    }
}

void GpuDevice::new_frame() {

    // Fence wait and reset
    VkFence* render_complete_fence = &vulkan_command_buffer_executed_fence[current_frame];

    if(vkGetFenceStatus(vulkan_device, *render_complete_fence) != VK_SUCCESS) {
        vkWaitForFences(vulkan_device, 1, render_complete_fence, VK_TRUE, UINT64_MAX);
    }

    vkResetFences(vulkan_device, 1, render_complete_fence);
    // command pool reset
    command_buffer_ring.reset_pools(current_frame);
    // Dynamic memory update
    const u32 used_size = dynamic_allocated_size - (dynamic_per_frame_size * previous_frame);
    dynamic_max_per_frame_size = puffin_max(used_size, dynamic_max_per_frame_size);
    dynamic_allocated_size = dynamic_per_frame_size * current_frame;

    // Descriptor Set Updates
    if(descriptor_set_updates.size) {
        for(u32 i = descriptor_set_updates.size - 1; i >= 0; i--) {
            DescriptorSetUpdate& update = descriptor_set_updates[i];

            update_descriptor_set_instant(update);

            update.frame_issued = u32_max;
            descriptor_set_updates.delete_swap(i);
        }
    }
}

void GpuDevice::present() {

    VkResult result = vkAcquireNextImageKHR(vulkan_device, vulkan_swapchain, UINT64_MAX,
                                            vulkan_image_acquired_semaphore, VK_NULL_HANDLE, &vulkan_image_index);

    if(result == VK_ERROR_OUT_OF_DATE_KHR) {
        resize_swapchain();

        // Advance frame counters that are skipped during this frame
        frame_counters_advance();

        return;
    }

    VkFence* render_complete_fence = &vulkan_command_buffer_executed_fence[current_frame];
    VkSemaphore* render_complete_semaphore = &vulkan_render_complete_semaphore[current_frame];

    // copy all commands
    VkCommandBuffer enqueued_command_buffers[4];
    for(u32 c = 0; c < num_queued_command_buffers; c++) {
        CommandBuffer* command_buffer = queued_command_buffers[c];

        enqueued_command_buffers[c] = command_buffer->vk_command_buffer;

        if(command_buffer->is_recording && command_buffer->current_render_pass && (command_buffer->current_render_pass->type != RenderPassType::Compute)) {
            vkCmdEndRenderPass(command_buffer->vk_command_buffer);
        }

        vkEndCommandBuffer(command_buffer->vk_command_buffer);
    }

    if(texture_to_update_bindless.size) {
        // Handle deferred writes to bindless textures.
        VkWriteDescriptorSet bindless_descriptor_writes[k_max_bindless_resources];
        VkDescriptorImageInfo bindless_image_info[k_max_bindless_resources];

        Texture* vk_dummy_texture = access_texture(dummy_texture);

        u32 current_write_index = 0;
        for(i32 it = texture_to_update_bindless.size - 1; it >= 0; it--) {
            ResourceUpdate& texture_to_update = texture_to_update_bindless[it];

            Texture* texture = access_texture({ texture_to_update.handle });
            VkWriteDescriptorSet& descriptor_write = bindless_descriptor_writes[current_write_index];
            descriptor_write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            descriptor_write.descriptorCount = 1;
            descriptor_write.dstArrayElement = texture_to_update.handle;
            descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_write.dstSet = vulkan_bindless_descriptor_set;
            descriptor_write.dstBinding = k_bindless_texture_binding;

            PASSERT(texture->handle.index == texture_to_update.handle);

            Sampler* vk_default_sampler = access_sampler(default_sampler);
            VkDescriptorImageInfo& descriptor_image_info = bindless_image_info[current_write_index];

            if(texture->sampler != nullptr) {
                descriptor_image_info.sampler = texture->sampler->vk_sampler;
            } else {
                descriptor_image_info.sampler = vk_default_sampler->vk_sampler;
            }

            descriptor_image_info.imageView = texture->vk_format != VK_FORMAT_UNDEFINED ? texture->vk_image_view : vk_dummy_texture->vk_image_view;
            descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            descriptor_write.pImageInfo = &descriptor_image_info;

            texture_to_update.current_frame = u32_max;

            texture_to_update_bindless.delete_swap(it);

            current_write_index++;
        }

        if(current_write_index) {
            vkUpdateDescriptorSets(vulkan_device, current_write_index, bindless_descriptor_writes, 0, nullptr);
        }

    }

    // Submit command buffers
    VkSemaphore wait_semaphores[] = { vulkan_image_acquired_semaphore };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = num_queued_command_buffers;
    submit_info.pCommandBuffers = enqueued_command_buffers;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = render_complete_semaphore; // telling present info we're good to present

    vkQueueSubmit(vulkan_queue, 1, &submit_info, *render_complete_fence);

    VkPresentInfoKHR present_info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = render_complete_semaphore;

    VkSwapchainKHR swapchains[] = { vulkan_swapchain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &vulkan_image_index;
    present_info.pResults = nullptr;
    result = vkQueuePresentKHR(vulkan_queue, &present_info);

    num_queued_command_buffers = 0;

    // GPU Timestamp resolve
    if(timestamps_enabled) {
        if(gpu_timestamp_manager->has_valid_queries()) {
            // Query GPU for timestamps
            const u32 query_offset = (current_frame * gpu_timestamp_manager->queries_per_frame) * 2;
            const u32 query_count = gpu_timestamp_manager->current_query * 2;
            vkGetQueryPoolResults(vulkan_device, vulkan_timestamp_query_pool, query_offset, query_count,
                                  sizeof(u64) * query_count * 2, &gpu_timestamp_manager->timestamps_data[query_offset],
                                  sizeof(gpu_timestamp_manager->timestamps_data[0]), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

            // Calculate and cache the elapsed time
            for(u32 i = 0; i < gpu_timestamp_manager->current_query; i++) {
                u32 index = (current_frame * gpu_timestamp_manager->queries_per_frame) + i;

                GPUTimestamp& timestamp = gpu_timestamp_manager->timestamps[index];

                double start = (double)gpu_timestamp_manager->timestamps_data[(index * 2)];
                double end = (double)gpu_timestamp_manager->timestamps_data[(index * 2) + 1];
                double range = end - start;
                double elapsed_time = range * gpu_timestamp_frequency;

                timestamp.elapsed_ms = elapsed_time;
                timestamp.frame_index = absolute_frame;
            }
        } else if(gpu_timestamp_manager->current_query) {
            p_print("Asymmetrical GPU queries, missing pop of some markers!\n");
        }

        gpu_timestamp_manager->reset();
        gpu_timestamp_reset = true;
    } else {
        gpu_timestamp_reset = false;
    }

    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized) {
        resized = false;
        resize_swapchain();

        // Advance frame counters that are skipped during this frame
        frame_counters_advance();

        return;
    }

    frame_counters_advance();

    // Resource deletion using reverse iteration and swap with last element
    if(resource_deletion_queue.size > 0) {
        for(i32 i = resource_deletion_queue.size - 1; i >= 0; i--){
            ResourceUpdate& resource_deletion = resource_deletion_queue[i];

            if(resource_deletion.current_frame == current_frame) {

                switch(resource_deletion.type) {
                    case ResourceDeletionType::Buffer:
                    {
                        destroy_buffer_instant(resource_deletion.handle);
                        break;
                    }
                    case ResourceDeletionType::Pipeline:
                    {
                        destroy_pipeline_instant(resource_deletion.handle);
                        break;
                    }
                    case ResourceDeletionType::RenderPass:
                    {
                        destroy_render_pass_instant(resource_deletion.handle);
                        break;
                    }
                    case ResourceDeletionType::DescriptorSet:
                    {
                        destroy_descriptor_set_instant(resource_deletion.handle);
                        break;
                    }
                    case ResourceDeletionType::DescriptorSetLayout:
                    {
                        destroy_descriptor_set_layout_instant(resource_deletion.handle);
                        break;
                    }
                    case ResourceDeletionType::Sampler:
                    {
                        destroy_sampler_instant( resource_deletion.handle );
                        break;
                    }
                    case ResourceDeletionType::ShaderState:
                    {
                        destroy_shader_state_instant( resource_deletion.handle );
                        break;
                    }
                    case ResourceDeletionType::Texture:
                    {
                        destroy_texture_instant( resource_deletion.handle );
                        break;
                    }
                }

                // mark resource as free
                resource_deletion.current_frame = u32_max;

                // swap element
                resource_deletion_queue.delete_swap(i);
            }
        }
    }
}

static VkPresentModeKHR to_vk_present_mode(PresentMode::Enum mode) {
    switch(mode) {
        case PresentMode::VSyncFast:
            return VK_PRESENT_MODE_MAILBOX_KHR;
        case PresentMode::VSyncRelaxed:
            return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        case PresentMode::Immediate:
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        case PresentMode::VSync:
        default:
            return VK_PRESENT_MODE_FIFO_KHR;
    }
}

void GpuDevice::set_present_mode(PresentMode::Enum mode) {
    // Request a certain mode and confirm that it is available. If not, use VK_PRESENT_MODE_FIFO_KHR which is mandatory
    u32 supported_count = 0;

    static VkPresentModeKHR supported_mode_allocated[8];
    vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan_physical_device, vulkan_window_surface, &supported_count, NULL);
    PASSERT(supported_count < 8);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan_physical_device, vulkan_window_surface, &supported_count, supported_mode_allocated);

    bool mode_found = false;
    VkPresentModeKHR requested_mode = to_vk_present_mode(mode);
    for(u32 j = 0; j < supported_count; j++) {
        if(requested_mode == supported_mode_allocated[j]) {
            mode_found = true;
            break;
        }
    }

    // Default to VK_PRESENT_MODE_FIFO_KHR that is guarenteed to always be supported
    vulkan_present_mode = mode_found ? requested_mode : VK_PRESENT_MODE_FIFO_KHR;

    // Use 4 for immediate??
    vulkan_swapchain_image_count = 3;

    present_mode = mode_found ? mode : PresentMode::VSync;
}

void GpuDevice::link_texture_sampler(TextureHandle texture, SamplerHandle sampler) {
    Texture* texture_vk = access_texture(texture);
    Sampler* sampler_vk = access_sampler(sampler);

    texture_vk->sampler = sampler_vk;
}

void GpuDevice::frame_counters_advance() {
    previous_frame = current_frame;
    current_frame = (current_frame + 1) % vulkan_swapchain_image_count;

    absolute_frame++;
}

void GpuDevice::queue_command_buffer(CommandBuffer* command_buffer) {
    queued_command_buffers[num_queued_command_buffers++] = command_buffer;
}

CommandBuffer* GpuDevice::get_command_buffer(QueueType::Enum type, bool begin) {
    CommandBuffer* cb = command_buffer_ring.get_command_buffer(current_frame, begin);

    // The first command buffer issued in the frame is used to reset the timestamp queries used
    if(gpu_timestamp_reset && begin) {
        // these are currently indices
        vkCmdResetQueryPool(cb->vk_command_buffer, vulkan_timestamp_query_pool, current_frame * gpu_timestamp_manager->queries_per_frame * 2,
                            gpu_timestamp_manager->queries_per_frame);

        gpu_timestamp_reset = false;
    }

    return cb;
}

CommandBuffer* GpuDevice::get_instant_command_buffer() {
    CommandBuffer* cb = command_buffer_ring.get_command_buffer_instant(current_frame, false);
    return cb;
}


// Resource Description Query //////////////////////////////

void GpuDevice::query_buffer(BufferHandle buffer, BufferDescription& out_description) {
    if(buffer.index != k_invalid_index) {
        const Buffer* buffer_data = access_buffer(buffer);

        out_description.name = buffer_data->name;
        out_description.size = buffer_data->size;
        out_description.type_flags = buffer_data->type_flags;
        out_description.usage = buffer_data->usage;
        out_description.parent_handle = buffer_data->parent_buffer;
        out_description.native_handle = (void*) &buffer_data->vk_buffer;
    }
}

void GpuDevice::query_texture(TextureHandle texture, TextureDescription& out_description) {
    if(texture.index != k_invalid_index) {
        const Texture* texture_data = access_texture(texture);

        out_description.width = texture_data->width;
        out_description.height = texture_data->height;
        out_description.depth = texture_data->depth;
        out_description.format = texture_data->vk_format;
        out_description.mipmaps = texture_data->mipmaps;
        out_description.type = texture_data->type;
        out_description.render_target = (texture_data->flags & TextureFlags::RenderTarget_mask) == TextureFlags::RenderTarget_mask;
        out_description.compute_access = (texture_data->flags & TextureFlags::Compute_mask) == TextureFlags::Compute_mask;
        out_description.native_handle = (void*)&texture_data->vk_image;
        out_description.name = texture_data->name;
    }
}

void GpuDevice::query_pipeline(PipelineHandle pipeline, PipelineDescription& out_description) {
    if(pipeline.index != k_invalid_index) {
        const Pipeline* pipeline_data = access_pipeline(pipeline);

        out_description.shader = pipeline_data->shader_state;
    }
}

void GpuDevice::query_sampler(SamplerHandle sampler, SamplerDescription& out_description) {
    if(sampler.index != k_invalid_index) {
        const Sampler* sampler_data = access_sampler(sampler);

        out_description.address_mode_u = sampler_data->address_mode_u;
        out_description.address_mode_v = sampler_data->address_mode_v;
        out_description.address_mode_w = sampler_data->address_mode_w;

        out_description.min_filter = sampler_data->min_filter;
        out_description.mag_filter = sampler_data->mag_filter;
        out_description.mip_filter = sampler_data->mip_filter;

        out_description.name = sampler_data->name;
    }
}

void GpuDevice::query_descriptor_set_layout(DescriptorSetLayoutHandle descriptor_set_layout, DescriptorSetLayoutDescription& out_description) {
    if(descriptor_set_layout.index != k_invalid_index) {
        const DescriptorSetLayout* descriptor_set_layout_data = access_descriptor_set_layout(descriptor_set_layout);

        const u32 num_bindings = descriptor_set_layout_data->num_bindings;
        for(size_t i = 0; i < num_bindings; i++) {
            out_description.bindings[i].name = descriptor_set_layout_data->bindings[i].name;
            out_description.bindings[i].type = descriptor_set_layout_data->bindings[i].type;
        }

        out_description.num_active_bindings = descriptor_set_layout_data->num_bindings;
    }
}

void GpuDevice::query_descriptor_set(DescriptorSetHandle descriptor_set, DescriptorSetDescription& out_description) {
    if(descriptor_set.index != k_invalid_index) {
        const DescriptorSet* descriptor_set_data = access_descriptor_set(descriptor_set);

        out_description.num_active_resources = descriptor_set_data->num_resources;
        for(u32 i = 0; i < out_description.num_active_resources; i++) {
            // Why commented out??
            // out_description.resources[ i ].data = descriptor_set_data->resources[ i ].data;
        }
    }
}

const RenderPassOutput& GpuDevice::get_render_pass_output(RenderPassHandle render_pass) const {
    const RenderPass* vulkan_render_pass = access_render_pass(render_pass);
    return vulkan_render_pass->output;
}


// Resource map/unmap /////////////////////////////////////////////

void* GpuDevice::map_buffer(const MapBufferParameters& parameters) {
    if(parameters.buffer.index == k_invalid_index) {
        return nullptr;
    }

    Buffer* buffer = access_buffer(parameters.buffer);

    if(buffer->parent_buffer.index == dynamic_buffer.index) {
        buffer->global_offset = dynamic_allocated_size;
        return dynamic_allocate(parameters.size == 0 ? buffer->size : parameters.size);
    }

    void* data;
    vmaMapMemory(vma_allocator, buffer->vma_allocation, &data);

    return data;
}

void GpuDevice::unmap_buffer(const MapBufferParameters& parameters) {
    if(parameters.buffer.index == k_invalid_index) {
        return;
    }

    Buffer* buffer = access_buffer(parameters.buffer);
    if(buffer->parent_buffer.index == dynamic_buffer.index) {
        return;
    }

    vmaUnmapMemory(vma_allocator, buffer->vma_allocation);
}

void* GpuDevice::dynamic_allocate(u32 size) {
    void* mapped_memory = dynamic_mapped_memory + dynamic_allocated_size;
    dynamic_allocated_size += (u32)puffin::memory_align(size, s_ubo_alignment);
    return mapped_memory;
}

void GpuDevice::set_buffer_global_offset(puffin::BufferHandle buffer, u32 offset) {
    if(buffer.index == k_invalid_index) {
        return;
    }

    Buffer* vulkan_buffer = access_buffer(buffer);
    vulkan_buffer->global_offset = offset;
}

u32 GpuDevice::get_gpu_timestamps(puffin::GPUTimestamp* out_timestamps) {
    return gpu_timestamp_manager->resolve(previous_frame, out_timestamps);
}

void GpuDevice::push_gpu_timestamp(puffin::CommandBuffer* command_buffer, cstring name) {
    if(!timestamps_enabled) {
        return;
    }

    u32 query_index = gpu_timestamp_manager->push(current_frame, name);
    vkCmdWriteTimestamp(command_buffer->vk_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        vulkan_timestamp_query_pool, query_index);
}

void GpuDevice::pop_gpu_timestamp(puffin::CommandBuffer* command_buffer) {
    if(!timestamps_enabled) {
        return;
    }

    u32 query_index = gpu_timestamp_manager->pop(current_frame);
    vkCmdWriteTimestamp(command_buffer->vk_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            vulkan_timestamp_query_pool, query_index);
}



// Utility methods ///////////////////////////

void check_result(VkResult result) {
    if(result == VK_SUCCESS) {
        return;
    }

    p_print("vulkan error: code(%u)", result);
    if(result < 0) {
        PASSERTM(false, "Vulkan error: aborting");
    }
}


// Device /////////////////////////////////////

BufferHandle GpuDevice::get_fullscreen_vertex_buffer() const {
    return fullscreen_vertex_buffer;
}

RenderPassHandle GpuDevice::get_swapchain_pass() const {
    return swapchain_pass;
}

TextureHandle GpuDevice::get_dummy_texture() const {
    return dummy_texture;
}

BufferHandle GpuDevice::get_dummy_constant_buffer() const {
    return dummy_constant_buffer;
}

void GpuDevice::resize(u16 width, u16 height) {
    swapchain_width = width;
    swapchain_height = height;

    resized = true;
}


// Resource Access //////////////////////////

ShaderState* GpuDevice::access_shader_state(puffin::ShaderStateHandle shader) {
    return (ShaderState*)shaders.access_resource(shader.index);
}

const ShaderState* GpuDevice::access_shader_state(puffin::ShaderStateHandle shader) const {
    return (const ShaderState*)shaders.access_resource(shader.index);
}

Texture* GpuDevice::access_texture(TextureHandle texture) {
    return (Texture*) textures.access_resource(texture.index);
}

const Texture* GpuDevice::access_texture(TextureHandle texture) const {
    return (const Texture*) textures.access_resource(texture.index);
}

Buffer* GpuDevice::access_buffer(BufferHandle buffer) {
    return (Buffer*)buffers.access_resource(buffer.index);
}

const Buffer* GpuDevice::access_buffer(BufferHandle buffer) const {
    return (const Buffer*)buffers.access_resource(buffer.index);
}

Pipeline* GpuDevice::access_pipeline(puffin::PipelineHandle pipeline) {
    return (Pipeline*)pipelines.access_resource(pipeline.index);
}

const Pipeline* GpuDevice::access_pipeline(puffin::PipelineHandle pipeline) const {
    return (const Pipeline*)pipelines.access_resource(pipeline.index);
}

Sampler* GpuDevice::access_sampler(SamplerHandle sampler) {
    return (Sampler*)samplers.access_resource(sampler.index);
}

const Sampler* GpuDevice::access_sampler(SamplerHandle sampler) const {
    return (const Sampler*)samplers.access_resource(sampler.index);
}

DescriptorSetLayout* GpuDevice::access_descriptor_set_layout(puffin::DescriptorSetLayoutHandle layout) {
    return (DescriptorSetLayout*) descriptor_set_layouts.access_resource(layout.index);
}

const DescriptorSetLayout* GpuDevice::access_descriptor_set_layout(puffin::DescriptorSetLayoutHandle layout) const {
    return (const DescriptorSetLayout*) descriptor_set_layouts.access_resource(layout.index);
}

DescriptorSetLayoutHandle GpuDevice::get_descriptor_set_layout_handle(PipelineHandle pipeline_handle,
                                                                      int layout_index) {
    Pipeline* pipeline = access_pipeline(pipeline_handle);
    PASSERT(pipeline != nullptr);

    return pipeline->descriptor_set_layout_handle[layout_index];
}

DescriptorSetLayoutHandle GpuDevice::get_descriptor_set_layout_handle(PipelineHandle pipeline_handle,
                                                                      int layout_index) const {
    const Pipeline* pipeline = access_pipeline(pipeline_handle);
    PASSERT(pipeline != nullptr);

    return pipeline->descriptor_set_layout_handle[layout_index];
}

DescriptorSet* GpuDevice::access_descriptor_set(DescriptorSetHandle set) {
    return (DescriptorSet*) descriptor_sets.access_resource(set.index);
}

const DescriptorSet* GpuDevice::access_descriptor_set(DescriptorSetHandle set) const {
    return (const DescriptorSet*) descriptor_sets.access_resource(set.index);
}

RenderPass* GpuDevice::access_render_pass(RenderPassHandle render_pass) {
    return (RenderPass*) render_passes.access_resource(render_pass.index);
}

const RenderPass* GpuDevice::access_render_pass(RenderPassHandle render_pass) const {
    return (const RenderPass*) render_passes.access_resource(render_pass.index);
}

// GPU TIMESTAMP MANAGER ////////////////////////////////////////

void GPUTimestampManager::init(Allocator* allocator_, u16 queries_per_frame_, u16 max_frames) {
    allocator = allocator_;
    queries_per_frame = queries_per_frame_;

    // Data is start, end in 2 u64 numbers
    const u32 k_data_per_query = 2;
    const size_t allocated_size = sizeof(GPUTimestamp) * queries_per_frame * max_frames + sizeof(u64) * queries_per_frame * max_frames * k_data_per_query;
    u8* memory = puffin_alloc_return_mem_pointer(allocated_size, allocator);

    timestamps = (GPUTimestamp*) memory;
    timestamps_data = (u64*)(memory + sizeof(GPUTimestamp) * queries_per_frame * max_frames);

    reset();
}

void GPUTimestampManager::shutdown() {
    puffin_free(timestamps, allocator);
}

void GPUTimestampManager::reset() {
    current_query = 0;
    parent_index = 0;
    current_frame_resolved = false;
    depth = 0;
}

bool GPUTimestampManager::has_valid_queries() const {
    return current_query > 0 && (depth == 0);
}

u32 GPUTimestampManager::resolve(u32 current_frame, GPUTimestamp* timestamps_to_fill) {
    puffin::memory_copy(timestamps_to_fill, &timestamps[current_frame * queries_per_frame], sizeof(GPUTimestamp) * current_query);
    return current_query;
}

u32 GPUTimestampManager::push(u32 current_frame, const char* name) {
    u32 query_index = (current_frame * queries_per_frame) + current_query;

    GPUTimestamp& timestamp = timestamps[query_index];
    timestamp.parent_index = (u16)parent_index;
    timestamp.start = query_index * 2;
    timestamp.end = timestamp.start + 1;
    timestamp.name = name;
    timestamp.depth = (u16)depth++;

    parent_index = current_query;
    current_query++;

    return (query_index * 2);
}

u32 GPUTimestampManager::pop( u32 current_frame ) {

    u32 query_index = ( current_frame * queries_per_frame ) + parent_index;
    GPUTimestamp& timestamp = timestamps[ query_index ];
    // Go up a level
    parent_index = timestamp.parent_index;
    --depth;

    return ( query_index * 2 ) + 1;
}

DeviceCreation& DeviceCreation::set_window( u32 width_, u32 height_, void* handle ) {
    width = ( u16 )width_;
    height = ( u16 )height_;
    window = handle;
    return *this;
}

DeviceCreation& DeviceCreation::set_allocator( Allocator* allocator_ ) {
    allocator = allocator_;
    return *this;
}

DeviceCreation& DeviceCreation::set_linear_allocator( StackAllocator* allocator ) {
    temporary_allocator = allocator;
    return *this;
}

} // puffin namespace

