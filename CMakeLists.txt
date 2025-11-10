cmake_minimum_required(VERSION 3.10)
project(Simple3DGame)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

if(NOT CMAKE_BUILD_TYPE)
      SET(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Generate root_directory.h
configure_file(configuration/root_directory.h.in ${CMAKE_BINARY_DIR}/root_directory.h)

link_directories(${CMAKE_SOURCE_DIR}/lib)

find_package(GLM REQUIRED)
find_package(GLFW3 REQUIRED)
find_package(ASSIMP REQUIRED)

if (WIN32)
    set(LIBS glfw3 opengl32 assimp)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
elseif (UNIX AND NOT APPLE)
  find_package(OpenGL REQUIRED)
  find_package(X11 REQUIRED)
  set(LIBS ${GLFW3_LIBRARY} X11 Xrandr Xinerama Xi Xxf86vm Xcursor GL dl pthread ${ASSIMP_LIBRARY})
  set (CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -ldl")
endif()

include_directories(${CMAKE_SOURCE_DIR}/includes ${CMAKE_BINARY_DIR})

# Add sources for glad and stb_image
add_library(GLAD src/glad.c)
add_library(STB_IMAGE src/stb_image.cpp)

add_executable(Simple3DGame src/game.cpp)

target_link_libraries(Simple3DGame GLAD STB_IMAGE ${LIBS})

# Copy shader files to build directory
file(GLOB SHADERS "src/*.vs" "src/*.fs")
add_custom_command(TARGET Simple3DGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    ${CMAKE_BINARY_DIR}/shaders
)

# Copy resources to build directory
add_custom_command(TARGET Simple3DGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources
    ${CMAKE_BINARY_DIR}/resources
)

# Windows dll
if(WIN32)
    file(GLOB DLLS "${CMAKE_SOURCE_DIR}/dlls/*.dll")
    add_custom_command(TARGET Simple3DGame POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${DLLS} $<TARGET_FILE_DIR:Simple3DGame>)
endif()
