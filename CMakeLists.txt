# 1. 声明最低要求的 CMake 版本
cmake_minimum_required(VERSION 3.20)
project(HybridRenderer_Basic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==========================================
# 依赖库查找与配置 (Dependencies)
# ==========================================
find_package(Vulkan REQUIRED)

include(FetchContent)
# 2. 自动下载并配置 GLFW (跨平台窗口和输入库)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 3. 自动下载并配置 GLM (只有头文件，无需编译，极速)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# 4. 自动下载并配置 stb_image (单头文件库，用于加载图片)
set(STB_IMAGE_DIR "${CMAKE_SOURCE_DIR}/external/stb")
set(STB_IMAGE_FILE "${STB_IMAGE_DIR}/stb_image.h")

if(NOT EXISTS ${STB_IMAGE_FILE})
    file(MAKE_DIRECTORY ${STB_IMAGE_DIR})
    message(STATUS "Downloading stb_image.h from GitHub...")
    file(DOWNLOAD 
        "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" 
        ${STB_IMAGE_FILE}
        SHOW_PROGRESS
    )
endif()

# 5. 自动下载tinyobjloader (单头文件库，用于加载 OBJ 模型)
set(TINYOBJ_DIR "${CMAKE_SOURCE_DIR}/external/tinyobjloader")
set(TINYOBJ_FILE "${TINYOBJ_DIR}/tiny_obj_loader.h")

if(NOT EXISTS ${TINYOBJ_FILE})
    file(MAKE_DIRECTORY ${TINYOBJ_DIR})
    message(STATUS "Downloading tiny_obj_loader.h from GitHub master branch...")
    file(DOWNLOAD 
        "https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/master/tiny_obj_loader.h" 
        ${TINYOBJ_FILE}
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS # 捕获下载状态
    )
    # 如果下载失败，在终端报错提示
    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download tinyobjloader. Please check your network or download it manually.")
    endif()
endif()


# ==========================================
# 核心引擎构建 (Target Creation) - 移到前面！
# ==========================================
file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.h")
add_executable(HybridRenderer ${SRC_FILES})

target_include_directories(HybridRenderer PRIVATE src ${STB_IMAGE_DIR} ${CMAKE_SOURCE_DIR}/external/tinyobjloader)
target_link_libraries(HybridRenderer PRIVATE Vulkan::Vulkan glfw glm::glm)

if (APPLE)
    target_link_libraries(HybridRenderer PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreFoundation")
endif()

# ==========================================
# Shader 离线编译管线 (GLSL -> SPIR-V)
# ==========================================
find_program(GLSLC_EXE NAMES glslc REQUIRED)

# 【核心修复 1】：精确指定输出目录
set(SHADER_BIN_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

# 【核心修复 2】：绝对不使用 GLOB！直接写死你要编译的文件列表。
# 注意：如果你之前把 shader 建在了 assets/shaders 里面，就用下面的路径。
# 如果你建在了根目录，请把 "assets/shaders/" 删掉！
set(SHADER_FILES 
    "${CMAKE_SOURCE_DIR}/assets/shaders/shader.vert"
    "${CMAKE_SOURCE_DIR}/assets/shaders/shader.frag"
)

set(SPIRV_FILES "")

foreach(SHADER_FILE ${SHADER_FILES})
    # 【核心修复 3】：在任何操作前，先验证硬盘上到底有没有这个文件！
    if(NOT EXISTS ${SHADER_FILE})
        message(FATAL_ERROR "❌ 致命错误: 找不到文件 [ ${SHADER_FILE} ]！请检查文件路径是否完全对应。")
    endif()

    get_filename_component(FILE_NAME ${SHADER_FILE} NAME)
    set(SPIRV_FILE "${SHADER_BIN_DIR}/${FILE_NAME}.spv")
    list(APPEND SPIRV_FILES ${SPIRV_FILE})

    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${GLSLC_EXE} ${SHADER_FILE} -o ${SPIRV_FILE}
        DEPENDS ${SHADER_FILE}
        COMMENT "Compiling shader: ${FILE_NAME} -> ${FILE_NAME}.spv"
    )
endforeach()

# 创建编译任务
add_custom_target(Shaders DEPENDS ${SPIRV_FILES})

# 【核心修复 4】：强行将主程序与 Shader 任务绑定！
# 必须保证先编译 Shader，再运行主程序。
add_dependencies(HybridRenderer Shaders)