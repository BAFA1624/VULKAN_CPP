#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <time.h>
#include <vector>

#define NDEBUG

class HelloTriangleApp
{
    public:
    HelloTriangleApp( const uint32_t width = 800, const uint32_t height = 600,
                      const bool enable_validation_layers = false ) :
        m_width( width ),
        m_height( height ),
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
    bool                            m_enable_validation_layers;
    const std::vector<const char *> m_validation_layers{
        "VK_LAYER_KHRONOS_validation"
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
    void init_vulkan() { create_instance(); }
    void main_loop() {
        while ( !glfwWindowShouldClose( m_window ) ) { glfwPollEvents(); }
    }
    void cleanup() {
        vkDestroyInstance( m_instance, nullptr );
        glfwDestroyWindow( m_window );
        glfwTerminate();
    }
    void setup_debug_messenger() {
        if ( !m_enable_validation_layers ) {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT create_info{};
        create_info.sType =
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = debug_callback;
        create_info.pUserData = nullptr; // optional
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

        if ( m_enable_validation_layers ) {
            create_info.enabledLayerCount =
                static_cast<uint32_t>( m_validation_layers.size() );
            create_info.ppEnabledLayerNames = m_validation_layers.data();
        }
        else {
            create_info.enabledLayerCount = 0;
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
        void *                                       p_user_data ) {
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
