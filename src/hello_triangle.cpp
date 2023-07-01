#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <time.h>
#include <vector>

#define NDEBUG

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
    bool                            m_enable_validation_layers;
    const std::vector<const char *> m_validation_layers{
        "VK_LAYER_KHRONOS_validation"
    };

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphics_family;
        bool is_complete() { return graphics_family.has_value(); };
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
        create_instance();
        setup_debug_messenger();
        pick_physical_device();
    }
    void main_loop() {
        while ( !glfwWindowShouldClose( m_window ) ) { glfwPollEvents(); }
    }
    void cleanup() {
        if ( m_enable_validation_layers ) {
            destroy_debug_utils_messenger_EXT( m_instance, m_debug_messenger,
                                               nullptr );
        }
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

    [[nodiscard]] static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT       message_severity,
        VkDebugUtilsMessageTypeFlagsEXT              message_type,
        const VkDebugUtilsMessengerCallbackDataEXT * p_callback_data,
        void *                                       p_user_data ) noexcept {
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
    [[nodiscard]] int
    rate_device_suitability( const VkPhysicalDevice device ) const noexcept {
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
            int score = rate_device_suitability( device );
            device_candidates.insert( std::make_pair( score, device ) );
        }

        // Check if the best candidate is suitable at all
        if ( device_candidates.rbegin()->first > 0 ) {
            m_physical_device = device_candidates.rbegin()->second;
        }
        else {
            std::runtime_error( "Failed to find suitable GPU." );
        }
    }
    [[nodiscard]] struct QueueFamilyIndices
    find_queue_families( VkPhysicalDevice device ) {
        QueueFamilyIndices indices;

        uint32_t queue_family_count{ 0 };
        vkGetPhysicalDeviceQueueFamilyProperties( device, &queue_family_count,
                                                  nullptr );

        std::vector<VkQueueFamilyProperties> queue_families(
            queue_family_count );
        vkGetPhysicalDeviceQueueFamilyProperties( device, &queue_family_count,
                                                  queue_families.data() );

        int i{ 0 };
        for ( const auto & queue_family : queue_families ) {
            if ( queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
                indices.graphics_family = i;
            }

            if ( indices.is_complete() ) {
                break;
            }

            i++;
        }

        return indices;
    }
    [[nodiscard]] bool is_device_suitable( VkPhysicalDevice device ) {
        QueueFamilyIndices indices{ find_queue_families( device ) };
        return indices.is_complete();
    }
};

int
main() {
    HelloTriangleApp app( 800, 600, true );
    std::cout << VK_ERROR_INCOMPATIBLE_DRIVER << std::endl;
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
