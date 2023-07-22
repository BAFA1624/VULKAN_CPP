#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <time.h>
#include <vector>

#define NDEBUG

// Helper functions

static std::vector<char>
read_file( const std::string_view filename ) {
    std::ifstream file( filename.data(), std::ios::ate | std::ios::binary );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Couldn't open file: "
                                  + std::string{ filename } );
    }

    // Get filesize + buffer for file
    const size_t      file_size = file.tellg();
    std::vector<char> file_buffer( file_size );

    // Start of file
    file.seekg( 0 );
    // Read contents
    file.read( file_buffer.data(), file_size );

    file.close();

    return file_buffer;
}

VkShaderModule
create_shader_module( const VkDevice device, const std::vector<char> & code ) {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t *>( code.data() );

    VkShaderModule shader_module{};
    if ( vkCreateShaderModule( device, &create_info, nullptr, &shader_module )
         != VK_SUCCESS ) {
        throw std::runtime_error( "Failed to create shader module." );
    }

    return shader_module;
}

// External debug functions

VkResult
create_debug_utils_messenger_EXT(
    VkInstance                                 instance,
    const VkDebugUtilsMessengerCreateInfoEXT * p_create_info,
    const VkAllocationCallbacks *              p_allocator,
    VkDebugUtilsMessengerEXT *                 p_debug_messenger ) {
    const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" ) );

    if ( func != nullptr ) {
        return func( instance, p_create_info, p_allocator, p_debug_messenger );
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void
destroy_debug_utils_messenger_EXT( VkInstance               instance,
                                   VkDebugUtilsMessengerEXT debug_messenger,
                                   const VkAllocationCallbacks * p_allocator ) {
    const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" ) );

    if ( func != nullptr ) {
        return func( instance, debug_messenger, p_allocator );
    }
}

void
populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT & create_info,
    PFN_vkDebugUtilsMessengerCallbackEXT debug_callback ) {
    create_info = VkDebugUtilsMessengerCreateInfoEXT{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                              | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                              | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback;
    create_info.pUserData = nullptr; // optional
}

// Helper structs & related functions

struct QueueFamilyIndices
{
    std::optional<uint32_t> m_graphics_family;
    std::optional<uint32_t> m_present_family;

    [[nodiscard]] constexpr auto is_complete() const noexcept {
        return m_graphics_family.has_value() && m_present_family.has_value();
    }

    [[nodiscard]] constexpr auto graphics_family() const {
        return m_graphics_family.value();
    }
    void graphics_family( const auto i ) noexcept { m_graphics_family = i; }

    [[nodiscard]] constexpr auto present_family() const {
        return m_present_family.value();
    }
    void present_family( const auto i ) noexcept { m_present_family = i; }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   present_modes;
};

struct SwapChainSupportDetails
query_swapchain_support( VkPhysicalDevice & device, VkSurfaceKHR & surface )
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, surface,
                                               &details.capabilities );

    uint32_t format_count{ 0 };
    vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, &format_count,
                                          nullptr );

    if ( format_count != 0 ) {
        details.formats.resize( format_count );
        vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, &format_count,
                                              details.formats.data() );
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface,
                                               &present_mode_count, nullptr );

    if ( present_mode_count != 0 ) {
        details.present_modes.resize( present_mode_count );
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &present_mode_count,
            details.present_modes.data() );
    }

    return details;
}

// HelloTriangleApp class

class HelloTriangleApp
{
    public:
    HelloTriangleApp( const uint32_t width = 800, const uint32_t height = 600,
                      const bool enable_validation_layers = false ) :
        m_width( width ),
        m_height( height ),
        m_physical_device( VK_NULL_HANDLE ),
        m_enable_validation_layers( enable_validation_layers ) {}
    void run() {
        init_window();
        init_vulkan();
        main_loop();
        cleanup();
    }

    private:
    uint32_t                        m_width, m_height;
    GLFWwindow *                    m_window;
    VkInstance                      m_instance;
    VkDebugUtilsMessengerEXT        m_debug_messenger;
    VkPhysicalDevice                m_physical_device;
    VkDevice                        m_device;
    VkQueue                         m_graphics_queue;
    VkSurfaceKHR                    m_surface;
    VkQueue                         m_present_queue;
    VkSwapchainKHR                  m_swapchain;
    std::vector<VkImage>            m_swapchain_images;
    std::vector<VkImageView>        m_swapchain_image_views;
    VkFormat                        m_swapchain_image_format;
    VkExtent2D                      m_swapchain_extent;
    VkRenderPass                    m_render_pass;
    VkPipelineLayout                m_pipeline_layout;
    bool                            m_enable_validation_layers;
    const std::vector<const char *> m_validation_layers{
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char *> m_device_extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    void init_window() {
        glfwInit();
        // GLFW originally designed to use an OpenGL context,
        // this tells it not to.
        glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
        // Handling resizing takes care, so disable for now.
        glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

        // Create the window.
        m_window =
            glfwCreateWindow( m_width, m_height, "Vulkan", nullptr, nullptr );
    }
    void init_vulkan() {
        try {
            create_instance();
            setup_debug_messenger();
            create_surface();
            pick_physical_device();
            create_logical_device();
            create_swap_chain();
            create_image_views();
            create_render_pass();
            create_graphics_pipeline();
        }
        catch ( const std::exception & err ) {
            std::cerr << err.what() << std::endl;
            return;
        }
    }
    void main_loop() {
        while ( !glfwWindowShouldClose( m_window ) ) { glfwPollEvents(); }
    }
    void cleanup() {
        vkDestroyPipelineLayout( m_device, m_pipeline_layout, nullptr );
        vkDestroyRenderPass( m_device, m_render_pass, nullptr );
        for ( auto image_view : m_swapchain_image_views ) {
            vkDestroyImageView( m_device, image_view, nullptr );
        }
        vkDestroySwapchainKHR( m_device, m_swapchain, nullptr );
        vkDestroyDevice( m_device, nullptr );
        if ( m_enable_validation_layers ) {
            destroy_debug_utils_messenger_EXT( m_instance, m_debug_messenger,
                                               nullptr );
        }
        vkDestroySurfaceKHR( m_instance, m_surface, nullptr );
        vkDestroyInstance( m_instance, nullptr );
        glfwDestroyWindow( m_window );
        glfwTerminate();
    }
    void setup_debug_messenger() {
        if ( !m_enable_validation_layers ) {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT create_info;
        populate_debug_messenger_create_info( create_info, debug_callback );

        if ( create_debug_utils_messenger_EXT( m_instance, &create_info,
                                               nullptr, &m_debug_messenger ) ) {
            throw std::runtime_error( "Debug messenger setup failed." );
        }
    }

    [[nodiscard]] auto check_validation_layer_support() {
        // Get available layers
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties( &layer_count, nullptr );

        std::vector<VkLayerProperties> available_layers( layer_count );
        vkEnumerateInstanceLayerProperties( &layer_count,
                                            available_layers.data() );

        // Check all validation layers exist
        for ( const auto layer_name : m_validation_layers ) {
            bool layer_found = false;
            for ( const auto & layer_properties : available_layers ) {
                if ( strcmp( layer_name, layer_properties.layerName ) == 0 ) {
                    layer_found = true;
                    break;
                }
            }
            if ( !layer_found ) {
                return false;
            }
        }
        return true;
    }
    [[nodiscard]] auto get_required_extensions() const {
        uint32_t      glfw_extension_count = 0;
        const char ** glfw_extensions = nullptr;

        glfw_extensions =
            glfwGetRequiredInstanceExtensions( &glfw_extension_count );

        std::vector<const char *> extensions(
            glfw_extensions, glfw_extensions + glfw_extension_count );

        if ( m_enable_validation_layers ) {
            extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
        }
#ifdef __APPLE__
        extensions.emplace_back(
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME );
#endif

        return extensions;
    }
    void create_instance() {
        if ( m_enable_validation_layers && !check_validation_layer_support() ) {
            throw std::runtime_error(
                "Validation layers requested with no support." );
        }

        // Technically optional
        auto app_info{ VkApplicationInfo{} };
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Hello Triangle";
        app_info.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
        app_info.pEngineName = "No Engine";
        app_info.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
        app_info.apiVersion = VK_MAKE_VERSION( 1, 0, 0 );

        // Mandatory, tells Vulkan driver about global extensions & validation
        // layers
        auto create_info{ VkInstanceCreateInfo{} };
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;

#ifdef __APPLE__
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

        const auto extensions = get_required_extensions();
        create_info.enabledExtensionCount =
            static_cast<uint32_t>( extensions.size() );
        create_info.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
        if ( m_enable_validation_layers ) {
            create_info.enabledLayerCount =
                static_cast<uint32_t>( m_validation_layers.size() );
            create_info.ppEnabledLayerNames = m_validation_layers.data();

            populate_debug_messenger_create_info( debug_create_info,
                                                  debug_callback );
            create_info.pNext =
                reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT *>(
                    &debug_create_info );
        }
        else {
            create_info.enabledLayerCount = 0;
            create_info.pNext = nullptr;
        }

        // Create the Vulkan instance
        VkResult result =
            vkCreateInstance( &create_info, nullptr, &m_instance );
        if ( result != VK_SUCCESS ) {
            throw std::runtime_error(
                "Failed to create Vulkan instance, error code: "
                + std::to_string( result ) );
        }
    }
    void create_logical_device() noexcept {
        QueueFamilyIndices indices{ find_queue_families( m_physical_device ) };

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::set<uint32_t> unique_queue_families{ indices.graphics_family(),
                                                  indices.present_family() };

        float queue_priority{ 1.0f };
        for ( const auto & queue_family : unique_queue_families ) {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType =
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queue_priority;

            queue_create_infos.push_back( queue_create_info );
        }

        // For use later with more advanced Vulkan features
        VkPhysicalDeviceFeatures device_features{};

        VkDeviceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount =
            static_cast<uint32_t>( queue_create_infos.size() );
        create_info.pQueueCreateInfos = queue_create_infos.data();
        create_info.queueCreateInfoCount = 1;
        create_info.pEnabledFeatures = &device_features;
        create_info.enabledExtensionCount =
            static_cast<uint32_t>( m_device_extensions.size() );
        create_info.ppEnabledExtensionNames = m_device_extensions.data();

        if ( m_enable_validation_layers ) {
            create_info.enabledLayerCount =
                static_cast<uint32_t>( m_validation_layers.size() );
            create_info.ppEnabledLayerNames = m_validation_layers.data();
        }
        else {
            create_info.enabledLayerCount = 0;
        }

        if ( vkCreateDevice( m_physical_device, &create_info, nullptr,
                             &m_device )
             != VK_SUCCESS ) {
            std::cerr << "Failed to create logical device." << std::endl;
            exit( -1 );
            // throw std::runtime_error( "Failed to create logical device." );
        }

        vkGetDeviceQueue( m_device, indices.graphics_family(), 0,
                          &m_graphics_queue );
        vkGetDeviceQueue( m_device, indices.present_family(), 0,
                          &m_present_queue );
    }
    void create_surface() {
        if ( glfwCreateWindowSurface( m_instance, m_window, nullptr,
                                      &m_surface )
             != VK_SUCCESS ) {
            throw std::runtime_error( "Failed to create window surface." );
        }
    }
    [[nodiscard]] static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT           message_severity,
        [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT *     p_callback_data,
        [[maybe_unused]] void * p_user_data ) noexcept {
        const time_t current_time{ time( NULL ) };
        const auto   time_str{ asctime( gmtime( &current_time ) ) };

        std::ofstream logfile( __FILE__ "_log.txt", std::ios::app );
        logfile << time_str << ": Validation layer - "
                << p_callback_data->pMessage << std::endl;

        if ( message_severity
             >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ) {
            std::cerr << time_str << ": Validation layer - "
                      << p_callback_data->pMessage << std::endl;
        }

        return VK_FALSE;
    }
    [[nodiscard]] const auto
    check_device_extension_support( VkPhysicalDevice device ) const noexcept {
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties( device, nullptr, &extension_count,
                                              nullptr );

        std::vector<VkExtensionProperties> available_extensions(
            extension_count );
        vkEnumerateDeviceExtensionProperties( device, nullptr, &extension_count,
                                              available_extensions.data() );

        std::set<std::string> required_extensions( m_device_extensions.begin(),
                                                   m_device_extensions.end() );

        for ( const auto & extension : available_extensions ) {
            required_extensions.erase( extension.extensionName );
        }

        return required_extensions.empty();
    }
    [[nodiscard]] auto is_device_suitable( VkPhysicalDevice device ) {
        QueueFamilyIndices indices{ find_queue_families( device ) };

        const bool extensions_supported{ check_device_extension_support(
            device ) };

        bool swap_chain_adequate{ false };
        if ( extensions_supported ) {
            const auto swapchain_support{ query_swapchain_support(
                device, m_surface ) };
            swap_chain_adequate = !swapchain_support.formats.empty()
                                  && !swapchain_support.present_modes.empty();
        }

        return indices.is_complete() && extensions_supported
               && swap_chain_adequate;
    }
    [[nodiscard]] int
    rate_device_suitability( const VkPhysicalDevice device ) noexcept {
        if ( !is_device_suitable( device ) ) {
            return 0;
        }

        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties( device, &device_properties );

        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceFeatures( device, &device_features );

        int score{ 0 };

        if ( device_properties.deviceType
             == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
            score += 1000;
        }

        // Maximum possible size of texture affects graphics quality.
        score += device_properties.limits.maxImageDimension2D;

        // Application can't function without geometry shaders.
        if ( !device_features.geometryShader ) {
            return 0;
        }

        return score;
    }
    void pick_physical_device() {
        uint32_t device_count{ 0 };
        vkEnumeratePhysicalDevices( m_instance, &device_count, nullptr );

        if ( device_count == 0 ) {
            throw std::runtime_error(
                "Failed to find GPUs with Vulkan support." );
        }

        std::vector<VkPhysicalDevice> devices( device_count );
        vkEnumeratePhysicalDevices( m_instance, &device_count, devices.data() );

        // Ordered map to
        std::multimap<int, VkPhysicalDevice> device_candidates;
        for ( const auto & device : devices ) {
            const int score = rate_device_suitability( device );
            device_candidates.insert( std::make_pair( score, device ) );
        }

        // Check if the best candidate is suitable at all
#ifndef __APPLE__
        if ( device_candidates.rbegin()->first > 0 ) {
#else
        if ( device_candidates.rbegin()->first >= 0 ) {
#endif
            m_physical_device = device_candidates.rbegin()->second;
        }
        else {
            throw std::runtime_error( "Failed to find suitable GPU." );
        }
    }
    [[nodiscard]] struct QueueFamilyIndices
    find_queue_families( VkPhysicalDevice device ) {
        QueueFamilyIndices indices;
        uint32_t           queue_family_count{ 0 };
        vkGetPhysicalDeviceQueueFamilyProperties( device, &queue_family_count,
                                                  nullptr );

        std::vector<VkQueueFamilyProperties> queue_families(
            queue_family_count );
        vkGetPhysicalDeviceQueueFamilyProperties( device, &queue_family_count,
                                                  queue_families.data() );
        int i{ 0 };
        for ( const auto & queue_family : queue_families ) {
            if ( queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
                indices.graphics_family( i );
            }

            VkBool32 present_support{ false };
            vkGetPhysicalDeviceSurfaceSupportKHR( device, i, m_surface,
                                                  &present_support );

            if ( present_support ) {
                indices.present_family( i );
            }

            if ( indices.is_complete() ) {
                break;
            }

            i++;
        }

        if ( !indices.is_complete() ) {
            throw std::runtime_error(
                "Unable to find queues for all requirements." );
        }

        return indices;
    }
    [[nodiscard]] VkSurfaceFormatKHR choose_swap_surface_format(
        const std::vector<VkSurfaceFormatKHR> & available_formats ) {
        for ( const auto & available_format : available_formats ) {
            if ( available_format.format == VK_FORMAT_B8G8R8A8_SRGB
                 && available_format.colorSpace
                        == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
                return available_format;
            }
        }

        return available_formats[0];
    }
    [[nodiscard]] VkPresentModeKHR choose_swap_present_mode(
        const std::vector<VkPresentModeKHR> & available_present_modes ) {
        for ( const auto & present_mode : available_present_modes ) {
            if ( present_mode == VK_PRESENT_MODE_MAILBOX_KHR ) {
                return present_mode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    [[nodiscard]] VkExtent2D
    choose_swap_extent( const VkSurfaceCapabilitiesKHR & capabilities ) {
        if ( capabilities.currentExtent.width
             != std::numeric_limits<std::uint32_t>::max() ) {
            return capabilities.currentExtent;
        }
        else {
            int width, height;
            glfwGetFramebufferSize( m_window, &width, &height );

            VkExtent2D actual_extent{
                std::clamp( static_cast<std::uint32_t>( width ),
                            capabilities.minImageExtent.width,
                            capabilities.maxImageExtent.width ),
                std::clamp( static_cast<std::uint32_t>( height ),
                            capabilities.minImageExtent.height,
                            capabilities.maxImageExtent.height )
            };

            return actual_extent;
        }
    }
    void create_swap_chain() {
        SwapChainSupportDetails swap_chain_support =
            query_swapchain_support( m_physical_device, m_surface );

        VkSurfaceFormatKHR surface_format =
            choose_swap_surface_format( swap_chain_support.formats );
        m_swapchain_image_format = surface_format.format;
        VkPresentModeKHR present_mode =
            choose_swap_present_mode( swap_chain_support.present_modes );
        VkExtent2D extent =
            choose_swap_extent( swap_chain_support.capabilities );
        m_swapchain_extent = extent;

        std::uint32_t image_count{ std::clamp(
            swap_chain_support.capabilities.minImageCount + 5,
            swap_chain_support.capabilities.minImageCount,
            swap_chain_support.capabilities.maxImageCount ) };

        VkSwapchainCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = m_surface;
        create_info.minImageCount = image_count;
        create_info.imageFormat = surface_format.format;
        create_info.imageColorSpace = surface_format.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = find_queue_families( m_physical_device );
        std::uint32_t      queue_family_indices[] = { indices.graphics_family(),
                                                      indices.present_family() };

        if ( indices.graphics_family() != indices.present_family() ) {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = queue_family_indices;
        }
        else {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            create_info.queueFamilyIndexCount = 0;
            create_info.pQueueFamilyIndices = nullptr;
        }

        create_info.preTransform =
            swap_chain_support.capabilities.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        create_info.presentMode = present_mode;
        create_info.clipped = VK_TRUE;

        create_info.oldSwapchain = VK_NULL_HANDLE;

        if ( vkCreateSwapchainKHR( m_device, &create_info, nullptr,
                                   &m_swapchain )
             != VK_SUCCESS ) {
            throw std::runtime_error( "Failed to create swapchain." );
        }

        vkGetSwapchainImagesKHR( m_device, m_swapchain, &image_count, nullptr );
        m_swapchain_images.resize( image_count );
        vkGetSwapchainImagesKHR( m_device, m_swapchain, &image_count,
                                 m_swapchain_images.data() );
    }
    void create_image_views() {
        m_swapchain_image_views.resize( m_swapchain_images.size() );

        for ( size_t i{ 0 }; i < m_swapchain_images.size(); ++i ) {
            VkImageViewCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = m_swapchain_images[i];

            // Specifies how the image data is interpreted
            create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format = m_swapchain_image_format;

            // The components field allows mapping of the colour channels
            create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            // subresourceRange is used to describe the images' purpose & which
            // part of the image should be accessed.
            create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel = 0;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount = 1;

            if ( vkCreateImageView( m_device, &create_info, nullptr,
                                    &m_swapchain_image_views[i] )
                 != VK_SUCCESS ) {
                throw std::runtime_error( "Failed to create image view." );
            }
        }
    }
    void create_graphics_pipeline() {
        // Setting up shader modules
        const auto vert_shader_code{ read_file( "shaders/triangle_vert.spv" ) };
        const auto frag_shader_code{ read_file( "shaders/triangle_frag.spv" ) };

        const auto vert_shader_mod =
            create_shader_module( m_device, vert_shader_code );
        const auto frag_shader_mod =
            create_shader_module( m_device, frag_shader_code );

        VkPipelineShaderStageCreateInfo vert_shader_create_info{};
        vert_shader_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_shader_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_shader_create_info.module = vert_shader_mod;
        vert_shader_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo frag_shader_create_info{};
        frag_shader_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_shader_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_shader_create_info.module = frag_shader_mod;
        frag_shader_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = {
            vert_shader_create_info, frag_shader_create_info
        };

        // Fixed functions

        std::vector<VkDynamicState> dynamic_states{ VK_DYNAMIC_STATE_VIEWPORT,
                                                    VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo dynamic_state_info{};
        dynamic_state_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_info.dynamicStateCount =
            static_cast<uint32_t>( dynamic_states.size() );
        dynamic_state_info.pDynamicStates = dynamic_states.data();

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = 0;
        vertex_input_info.pVertexBindingDescriptions = nullptr;
        vertex_input_info.vertexAttributeDescriptionCount = 0;
        vertex_input_info.pVertexAttributeDescriptions = nullptr;

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>( m_swapchain_extent.width );
        viewport.height = static_cast<float>( m_swapchain_extent.height );
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = m_swapchain_extent;

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        // Multisampling (Disabled for now)
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType =
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        // Color blending
        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;
        color_blend_attachment.srcColorBlendFactor =
            VK_BLEND_FACTOR_ONE; // Optional
        color_blend_attachment.dstColorBlendFactor =
            VK_BLEND_FACTOR_ZERO;                              // Optional
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        color_blend_attachment.srcAlphaBlendFactor =
            VK_BLEND_FACTOR_ONE; // Optional
        color_blend_attachment.dstAlphaBlendFactor =
            VK_BLEND_FACTOR_ZERO;                              // Optional
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

        VkPipelineColorBlendStateCreateInfo color_blend{};
        color_blend.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend.logicOpEnable = VK_FALSE;
        color_blend.logicOp = VK_LOGIC_OP_COPY;
        color_blend.attachmentCount = 1;
        color_blend.pAttachments = &color_blend_attachment;
        color_blend.blendConstants[0] = 0.0f;
        color_blend.blendConstants[1] = 0.0f;
        color_blend.blendConstants[2] = 0.0f;
        color_blend.blendConstants[3] = 0.0f;

        // Pipeline layout

        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 0;
        pipeline_layout_info.pSetLayouts = nullptr;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = nullptr;

        if ( vkCreatePipelineLayout( m_device, &pipeline_layout_info, nullptr,
                                     &m_pipeline_layout )
             != VK_SUCCESS ) {
            throw std::runtime_error( "Failed to create pipeline layout." );
        }

        vkDestroyShaderModule( m_device, vert_shader_mod, nullptr );
        vkDestroyShaderModule( m_device, frag_shader_mod, nullptr );
    }
    void create_render_pass() {
        VkAttachmentDescription color_attachment{};
        color_attachment.format = m_swapchain_image_format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_ref{};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;

        VkRenderPassCreateInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;

        if ( vkCreateRenderPass( m_device, &render_pass_info, nullptr,
                                 &m_render_pass )
             != VK_SUCCESS ) {
            throw std::runtime_error( "Failed to create render pass." );
        }
    }
};

int
main() {
    HelloTriangleApp app( 800, 600, true );
    try {
        app.run();
    }
    catch ( const std::exception & err ) {
        std::cerr << "ERROR: " << err.what() << std::endl;
        return EXIT_FAILURE;
    }

    uint32_t extension_count{ 0 };
    vkEnumerateInstanceExtensionProperties( nullptr, &extension_count,
                                            nullptr );
    std::vector<VkExtensionProperties> extensions( extension_count );
    vkEnumerateInstanceExtensionProperties( nullptr, &extension_count,
                                            extensions.data() );

    /*std::cout << "Available extensions:" << std::endl;
    for ( const auto & extension : extensions ) {
        std::cout << "\t" << extension.extensionName << std::endl;
    }*/

    return EXIT_SUCCESS;
}
