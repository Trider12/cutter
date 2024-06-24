cmake_minimum_required(VERSION 3.15)

project(cutter)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(
        /W4
        /WX
        /external:I ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
        /external:W0
        /external:templates-
    )
else()
    # TODO
endif()

add_executable(cutter)

target_sources(cutter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ShaderUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Stb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/VkUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Vma.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imgui/backends/imgui_impl_vulkan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/meshoptimizer/src/indexgenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/meshoptimizer/src/quantization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/vk-bootstrap/src/VkBootstrap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/volk/volk.c
)

target_include_directories(cutter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include

    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cgltf
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/meshoptimizer/src
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/renderdoc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/shaderc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/vk-bootstrap/src
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/vma/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/volk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/vulkan/include
)

target_compile_definitions(cutter PRIVATE
    VK_NO_PROTOTYPES
)

if(WIN32)
    target_compile_definitions(cutter PRIVATE
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        VK_USE_PLATFORM_WIN32_KHR
    )

    target_link_options(cutter PRIVATE
        $<$<NOT:$<CONFIG:DEBUG>>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>
    )

    target_link_libraries(cutter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glfw/lib-vc2022/glfw3.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/shaderc/lib/shaderc_combined.lib
    )

    set_target_properties(cutter PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL) # hack to allow debug build (MDd) to link release libs (MD)
else()
    # TODO
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imgui/misc/fonts/Roboto-Medium.ttf
               ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Roboto-Medium.ttf COPYONLY)