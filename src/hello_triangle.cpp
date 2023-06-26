#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

class HelloTriangleApp
{
    public:
    HelloTriangleApp( const uint32_t width = 800,
                      const uint32_t height = 600 ) :
        m_width( width ), m_height( height ) {}
    void run() {
        init_window();
        init_vulkan();
        main_loop();
        cleanup();
    }

    private:
    uint32_t     m_width, m_height;
    GLFWwindow * m_window;
    VkInstance   m_instance;


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
    void init_vulkan() { createInstance(); }
    void main_loop() {
        while ( !glfwWindowShouldClose( m_window ) ) { glfwPollEvents(); }
    }
    void cleanup() {
        vkDestroyInstance( m_instance, nullptr );
        glfwDestroyWindow( m_window );
        glfwTerminate();
    }

    void createInstance() {
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


        uint32_t      glfw_extension_count{ 0 };
        const char ** glfw_extensions{ nullptr };

        glfw_extensions =
            glfwGetRequiredInstanceExtensions( &glfw_extension_count );

        std::vector<const char *> required_extensions;
        required_extensions.reserve( glfw_extension_count );

        for ( uint32_t i{ 0 }; i < glfw_extension_count; ++i ) {
            std::cout << "Extension " << i << ": " << glfw_extensions[i]
                      << std::endl;
            required_extensions.emplace_back( glfw_extensions[i] );
        }
#ifdef __APPLE__
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        required_extensions.emplace_back(
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME );
#endif

        create_info.enabledExtensionCount =
            static_cast<uint32_t>( required_extensions.size() );
        create_info.ppEnabledExtensionNames = required_extensions.data();

        create_info.enabledLayerCount = 0;

        // Create the Vulkan instance
        VkResult result =
            vkCreateInstance( &create_info, nullptr, &m_instance );
        if ( result != VK_SUCCESS ) {
            throw std::runtime_error(
                "Failed to create Vulkan instance, error code: "
                + std::to_string( result ) );
        }
    }
};

int
main() {
    HelloTriangleApp app;
    std::cout << VK_ERROR_INCOMPATIBLE_DRIVER << std::endl;
    try {
        app.run();
    }
    catch ( const std::exception & err ) {
        std::cerr << err.what() << std::endl;
        return EXIT_FAILURE;
    }


    uint32_t extension_count{ 0 };
    vkEnumerateInstanceExtensionProperties( nullptr, &extension_count,
                                            nullptr );
    std::vector<VkExtensionProperties> extensions( extension_count );
    vkEnumerateInstanceExtensionProperties( nullptr, &extension_count,
                                            extensions.data() );

    std::cout << "Available extensions:" << std::endl;
    for ( const auto & extension : extensions ) {
        std::cout << "\t" << extension.extensionName << std::endl;
    }

    return EXIT_SUCCESS;
}
