cmake_minimum_required(VERSION 3.22)

cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

project(particles)

# thirdparty

add_subdirectory(thirdparty/cmrc)

set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
add_subdirectory(thirdparty/glfw)

add_library(glad thirdparty/glad/src/glad.c)
target_include_directories(glad PUBLIC thirdparty/glad/include)

add_subdirectory(thirdparty/glm)

file(GLOB IMGUI_SOURCES thirdparty/imgui/*.cpp)
add_library(imgui ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC thirdparty/imgui)
target_link_libraries(imgui PUBLIC glfw)

add_library(stbi INTERFACE)
target_include_directories(stbi INTERFACE thirdparty/stb)

# shaders

file(GLOB_RECURSE SHADERS_SOURCES shaders/*.comp shaders/*.vert shaders/*.frag)
set(SHADERS_SPV "")
foreach(SHADER ${SHADERS_SOURCES})
    file(RELATIVE_PATH SHADER_REL ${CMAKE_CURRENT_SOURCE_DIR}/shaders ${SHADER})
    set(SHADER_OUT ${CMAKE_CURRENT_SOURCE_DIR}/build-shaders/${SHADER_REL}.spv)
    add_custom_command(
        OUTPUT ${SHADER_OUT}
        DEPENDS ${SHADER}
        COMMAND glslangValidator --target-env opengl -o ${SHADER_OUT} ${SHADER}
    )
    list(APPEND SHADERS_SPV ${SHADER_OUT})
endforeach()
add_custom_target(build_shaders DEPENDS ${SHADERS_SPV})

cmrc_add_resource_library(shaders_spv ${SHADERS_SPV} WHENCE build-shaders)
add_dependencies(shaders_spv build_shaders)

# assets

file(GLOB_RECURSE ASSET_FILES assets/*)
cmrc_add_resource_library(assets ${ASSET_FILES})

# executable

file(GLOB_RECURSE PROJECT_SOURCES src/*.cpp src/*.hpp)
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw glad glm::glm imgui stbi shaders_spv assets)
