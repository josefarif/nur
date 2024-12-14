cmake_minimum_required(VERSION 3.16)
project(nur VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

file(GLOB SOURCES src/*.cpp)

find_package(Vulkan REQUIRED)

set(SDL_DIR "${CMAKE_SOURCE_DIR}/ext/sdl")
add_subdirectory(${SDL_DIR} EXCLUDE_FROM_ALL)

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES})
else()
    add_executable(${PROJECT_NAME} ${SOURCES})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan SDL3::SDL3)

add_dependencies(${PROJECT_NAME} SDL3::SDL3)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WINDOWS)
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MACOSX)
elseif(UNIX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE LINUX)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
    target_compile_options(${PROJECT_NAME} PRIVATE -g)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)
    target_compile_options(${PROJECT_NAME} PRIVATE -O3 -flto)
    target_link_options(${PROJECT_NAME} PRIVATE -s -static -static-libgcc -static-libstdc++ -lpthread -lm)
endif()