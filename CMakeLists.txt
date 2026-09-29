cmake_minimum_required(VERSION 3.16)
project(Schwengine LANGUAGES CXX) # Named after your window title!

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Find Vulkan
find_package(Vulkan REQUIRED)

# 2. Define the source and header files
set(SOURCES
    core/main.cpp
    core/Window.cpp
    core/camera.cpp
    math/MathUtils.cpp
    renderer/VulkanContext.cpp
    renderer/Pipeline.cpp
    renderer/Descriptors.cpp
    renderer/tiny_obj_loader.cpp
    renderer/stb_image.cpp
)

set(HEADERS
    core/Window.h
    core/camera.h
    math/Array.h
    math/Matrix4x4.h
    math/MathUtils.h
    math/Vector2.h
    math/Vector4.h
    renderer/Vertex.h
    renderer/VulkanContext.h
    renderer/Pipeline.h
    renderer/Descriptors.h
)

set(IMGUI_SOURCES
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/imgui_impl_win32.cpp
    vendor/imgui/imgui_impl_vulkan.cpp
)

# 3. Create the executable
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${IMGUI_SOURCES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# 4. Handle Precompiled Headers (The "Missing Executable" Fix)
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)
    
    # EXEMPT third-party libraries from PCH so MSVC doesn't crash the build
    set_source_files_properties(renderer/tiny_obj_loader.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    set_source_files_properties(renderer/stb_image.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    set_source_files_properties(${IMGUI_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()

# 5. Include Directories & Definitions
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/math
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
)

target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR NOMINMAX)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)

# Automatically copy the Assets folder to the executable directory after every build
add_custom_command(TARGET Schwengine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/Assets"
    "$<TARGET_FILE_DIR:Schwengine>/Assets"
)

# Keep the folder tree clean in IDEs
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES} ${HEADERS})


# =====================================================================
# SMART SHADER COMPILATION
# =====================================================================

find_program(GLSLC_EXECUTABLE glslc REQUIRED)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")

# We will collect all successfully generated shader outputs here
set(COMPILED_SHADERS "")

# MACRO: Only compiles the shader if the source file actually exists
macro(compile_shader SRC_FILE OUT_NAME)
    set(FULL_SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SRC_FILE}")
    if(EXISTS ${FULL_SRC_PATH})
        set(OUT_PATH "${CMAKE_BINARY_DIR}/shaders/${OUT_NAME}")
        add_custom_command(
            OUTPUT ${OUT_PATH}
            COMMAND ${GLSLC_EXECUTABLE} ${FULL_SRC_PATH} -o ${OUT_PATH}
            DEPENDS ${FULL_SRC_PATH}
            COMMENT "Compiling Shader: ${SRC_FILE} -> shaders/${OUT_NAME}"
        )
        list(APPEND COMPILED_SHADERS ${OUT_PATH})
    else()
        message(STATUS "Skipping shader compilation for ${SRC_FILE} (File not found).")
    endif()
endmacro()

# Try to compile standard shaders
compile_shader("shader.vert" "vert.spv")
compile_shader("shader.frag" "frag.spv")

# Try to compile optional tessellation shaders
compile_shader("shader.tesc" "tesc.spv")
compile_shader("shader.tese" "tese.spv")

# Tie the successfully compiled binary outputs directly to the main target loop
if(COMPILED_SHADERS)
    add_custom_target(CompileShaders ALL DEPENDS ${COMPILED_SHADERS})
    add_dependencies(${PROJECT_NAME} CompileShaders)
endif()