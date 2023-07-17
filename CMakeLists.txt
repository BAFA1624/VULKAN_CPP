cmake_minimum_required(VERSION 3.26)

project(VULKAN_CPP)

set(CMAKE_CXX_COMPILER g++-13)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add -v to see full build details
set(CMAKE_CXX_FLAGS_DEBUG "${LDFLAGS_DIR} -v -Wall -Wextra -pedantic -O0 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "${LDFLAGS_DIR} -Wall -Wextra -pedantic -Werror -O3")

add_subdirectory(src)
include_directories(./include)

find_package(glm REQUIRED HINTS ${DLIB_PATH})
find_package(glfw3 REQUIRED HINTS ${DLIB_PATH})

find_library(vulkan_lib vulkan REQUIRED HINTS ENV ${DLIB_PATH})

# I think these are needed on linux builds

#find_library(X11_lib libX11 REQUIRED HINTS ENV ${DLIB_PATH})
#find_library(xi_lib libXi REQUIRED HINTS ENV ${DLIB_PATH})
#find_library(xrandr_lib libXrandr REQUIRED HINTS ENV ${DLIB_PATH})
#find_library(xxf86vm_lib libXxf86vm REQUIRED HINTS ENV ${DLIB_PATH})

# find_library(X_11 X11 PATHS ${DLIB_PATH})
# find_library(LXXF86VM lXxf86vm PATHS ${DLIB_PATH})
# find_library(XRANDR Xrandr PATHS ${DLIB_PATH})
# find_library(XI Xi PATHS ${DLIB_PATH})

set(CHECK_SETUP check_setup)

add_executable(${CHECK_SETUP} src/check_setup.cpp)
target_link_libraries(${CHECK_SETUP} dl pthread ${vulkan_lib} ${GLM_LIBRARIES} glfw)

# add_custom_command(TARGET ${CHECK_SETUP})

# Set up for target to compile shaders

# Set shader compiler/validator
set(GLSL_VALIDATOR glslc)

# Find src files
file(GLOB_RECURSE GLSL_SRC_FILES
    "src/shaders/*.frag"
    "src/shaders/*.vert"
)

# For each shader src file, compile glsl -> spirv
foreach(GLSL ${GLSL_SRC_FILES})
    #get_filename_component(FILE_NAME ${GLSL} NAME)
    cmake_path(GET GLSL STEM LAST_ONLY FILE_NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
        COMMAND ${GLSL_VALIDATOR} ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)

# Same here, linux stuff. At some point will work out some sort of select
# for different OS's.
#target_link_libraries(main dl pthread ${X11_lib} ${lxxf86vm_lib} ${xrandr_lib} ${xi_lib} glm glfw3 ${vulkan_lib})

add_executable(hello_triangle src/hello_triangle.cpp)
target_link_libraries(hello_triangle dl pthread ${vulkan_lib} ${GLM_LIBRARIES} glfw)

add_dependencies(hello_triangle shaders)