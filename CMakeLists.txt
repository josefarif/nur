cmake_minimum_required(VERSION 3.30)

project(nur VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# SDL disabled features
set(SDL_GPU OFF)
set(SDL_AUDIO OFF)
set(SDL_RENDER OFF)
set(SDL_CAMERA OFF)
set(SDL_DIALOG OFF)
set(SDL_ASSEMBLY OFF)
set(SDL_STATIC ON)
set(SDL_SHARED OFF)
set(SDL_OFFSCREEN OFF)
set(SDL_OPENGL OFF)
set(SDL_OPENGLES OFF)
set(SDL_GCC_ATOMICS OFF)
set(SDL_DIRECTX OFF)
set(SDL_DUMMYVIDEO OFF)

file(GLOB_RECURSE SOURCES src/*.c)

# Must have Vulkan SDK installed
find_package(Vulkan REQUIRED)

add_subdirectory("${CMAKE_SOURCE_DIR}/ext/flecs")
add_subdirectory("${CMAKE_SOURCE_DIR}/ext/sdl")
add_subdirectory("${CMAKE_SOURCE_DIR}/ext/vma")

add_compile_definitions(
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_PATCH=${PROJECT_VERSION_PATCH}
    GAME_NAME="Nur Game"
    $<$<PLATFORM_ID:Windows>:WINDOWS>
    $<$<PLATFORM_ID:Darwin>:MACOSX>
    $<$<PLATFORM_ID:Linux>:LINUX>
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

add_compile_options(
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-flto>
)

# architecture-specific SIMD extensions 
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|i686|AMD64|amd64")
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/arch:SSE2)
    else()
        add_compile_options(-msse2)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        add_compile_options(-mfpu=neon-fp16)
    else()
        add_compile_options(-mfpu=neon)
    endif()
endif()

add_link_options(
    -static
    -static-libgcc
    -static-libstdc++
    -lpthread
    -lm
    $<$<CONFIG:Release>:-s>
)

# Make WIN32 on windows (no console)
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES})
else()
    add_executable(${PROJECT_NAME} ${SOURCES})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/ext/sdl/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/ext/vma/include")

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan SDL3::SDL3 GPUOpen::VulkanMemoryAllocator)

add_dependencies(${PROJECT_NAME} SDL3::SDL3)
