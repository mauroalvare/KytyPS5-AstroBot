cmake_minimum_required(VERSION 3.22)
project(AstroFrame VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "AstroFrame currently supports Windows only.")
endif()
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    message(FATAL_ERROR "Configure with clang-cl and a Visual Studio x64 developer environment.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "AstroFrame requires a 64-bit build.")
endif()

option(ASTRO_FRAME_BUILD_CPU_TESTS "Build tests that use no Vulkan loader, GPU or window" ON)
option(ASTRO_FRAME_BUILD_GPU_TESTS "Build the optional SDL3/Vulkan presentation smoke test" OFF)
set(ASTRO_VULKAN_HEADERS "" CACHE PATH "Vulkan-Headers checkout or its include directory")
get_filename_component(ASTRO_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
find_path(ASTRO_VULKAN_INCLUDE_DIR
    NAMES vulkan/vulkan.h
    HINTS
        "${ASTRO_VULKAN_HEADERS}"
        "${ASTRO_VULKAN_HEADERS}/include"
        "${ASTRO_REPOSITORY_ROOT}/3rdparty/Vulkan-Headers/include"
        "$ENV{VULKAN_SDK}/Include"
        "$ENV{VULKAN_SDK}/include"
    DOC "Directory containing Vulkan headers, including vulkan/vk_layer.h"
)
if(NOT ASTRO_VULKAN_INCLUDE_DIR OR NOT EXISTS "${ASTRO_VULKAN_INCLUDE_DIR}/vulkan/vk_layer.h")
    message(FATAL_ERROR
        "Vulkan headers were not found. Initialize 3rdparty/Vulkan-Headers, install a Vulkan SDK, "
        "or set ASTRO_VULKAN_HEADERS to a Vulkan-Headers checkout or include directory."
    )
endif()

function(astro_frame_target target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE "${ASTRO_VULKAN_INCLUDE_DIR}")
    target_compile_definitions(${target} PRIVATE
        VK_NO_PROTOTYPES WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(${target} PRIVATE bcrypt)
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
endfunction()

add_library(astro_frame SHARED layer.cpp)
astro_frame_target(astro_frame)
target_link_options(astro_frame PRIVATE /EXPORT:vkNegotiateLoaderLayerInterfaceVersion)
add_custom_command(TARGET astro_frame POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/VK_LAYER_ASTRO_frame.json"
        "$<TARGET_FILE_DIR:astro_frame>/VK_LAYER_ASTRO_frame.json"
)
install(TARGETS astro_frame RUNTIME DESTINATION .)
install(FILES VK_LAYER_ASTRO_frame.json DESTINATION .)

if(ASTRO_FRAME_BUILD_CPU_TESTS)
    enable_testing()
    foreach(test IN ITEMS cpu_checks cpu_regressions shader_fixture_checks)
        add_executable(frame_${test} ${test}.cpp)
        astro_frame_target(frame_${test})
        add_test(NAME astro_frame_${test} COMMAND frame_${test})
        set_tests_properties(astro_frame_${test} PROPERTIES LABELS cpu TIMEOUT 30)
    endforeach()
endif()

if(ASTRO_FRAME_BUILD_GPU_TESTS)
    find_package(Vulkan REQUIRED)
    find_package(SDL3 CONFIG REQUIRED)
    add_executable(frame_smoke smoke.cpp)
    target_compile_features(frame_smoke PRIVATE cxx_std_20)
    if(TARGET SDL3::SDL3-static)
        target_link_libraries(frame_smoke PRIVATE SDL3::SDL3-static Vulkan::Vulkan)
    else()
        target_link_libraries(frame_smoke PRIVATE SDL3::SDL3 Vulkan::Vulkan)
    endif()
    add_dependencies(frame_smoke astro_frame)
    enable_testing()
    add_test(NAME astro_frame_gpu_smoke COMMAND frame_smoke)
    set_tests_properties(astro_frame_gpu_smoke PROPERTIES LABELS gpu TIMEOUT 60
        ENVIRONMENT "VK_LAYER_PATH=$<TARGET_FILE_DIR:astro_frame>;VK_INSTANCE_LAYERS=VK_LAYER_ASTRO_frame;ASTRO_FRAME_ASPECT=16:9"
    )
endif()
