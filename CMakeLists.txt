# 1. 声明最低要求的 CMake 版本
cmake_minimum_required(VERSION 3.20)

# 2. 定义工程名称和使用的语言 (CXX 代表 C++)
project(HybridRenderer_Basic LANGUAGES CXX)

# 3. 设置 C++ 标准为 C++20 (现代图形工程标配)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# 将动态库/静态库/可执行文件统一输出到 bin 目录下，保持工程整洁
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==========================================
# 依赖库查找与配置 (Dependencies)
# ==========================================

# 查找系统环境中的 Vulkan SDK
# REQUIRED 表示找不到直接报错停止编译
find_package(Vulkan REQUIRED)

# 2. 自动下载并配置 GLFW (替换掉之前的 add_subdirectory)
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4 # 使用最新稳定版
)
# 关闭 GLFW 自带的测试、文档和例子，加快我们的编译速度
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# ==========================================
# Shader 离线编译管线 (GLSL -> SPIR-V)
# ==========================================

# 1. 寻找 Vulkan SDK 自带的着色器编译器 glslc
find_program(GLSLC_EXE NAMES glslc REQUIRED)

# 2. 指定着色器源码目录和编译输出目录
set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
set(SHADER_BIN_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")

# 确保输出目录存在
file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

# 3. 收集所有的 shader 源码文件
file(GLOB SHADER_FILES "${SHADER_SRC_DIR}/*.vert" "${SHADER_SRC_DIR}/*.frag")

set(SPIRV_FILES "")

# 4. 遍历每个 shader 文件，为其生成编译命令
foreach(SHADER_FILE ${SHADER_FILES})
    get_filename_component(FILE_NAME ${SHADER_FILE} NAME)
    set(SPIRV_FILE "${SHADER_BIN_DIR}/${FILE_NAME}.spv")
    list(APPEND SPIRV_FILES ${SPIRV_FILE})

    # 当源码文件改变时，自动触发 glslc 编译
    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${GLSLC_EXE} ${SHADER_FILE} -o ${SPIRV_FILE}
        DEPENDS ${SHADER_FILE}
        COMMENT "Compiling shader: ${FILE_NAME} -> ${FILE_NAME}.spv"
    )
endforeach()

# 5. 创建一个自定义的 Target 专门管理 Shader 编译
add_custom_target(Shaders DEPENDS ${SPIRV_FILES})

# ==========================================
# 核心引擎构建 (Target Creation)
# ==========================================

# 收集 src 目录下的所有源文件 (实际大型工程中推荐手动列出文件，这里为了方便起步)
file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.h")

# 创建可执行文件 Target
add_executable(HybridRenderer ${SRC_FILES})

# ==========================================
# 链接依赖与属性配置 (Target Configuration)
# ==========================================

# 为 Target 添加头文件搜索路径
# PRIVATE 表示这些头文件仅本项目内部使用，不会传染给依赖本项目的其他模块
target_include_directories(HybridRenderer PRIVATE src)

# 链接外部库 (这是 Modern CMake 的核心)
# PUBLIC 和 PRIVATE 定义了依赖的可见性
target_link_libraries(HybridRenderer
    PRIVATE
        Vulkan::Vulkan      # 链接 Vulkan SDK (现代 CMake 自动处理头文件和库路径)
        glfw                # 链接在 external 中编译的 GLFW
)

# 针对 Apple 平台的特殊处理 (确保 Mac 下能正确调用底层框架)
if (APPLE)
    target_link_libraries(HybridRenderer
        PRIVATE
            "-framework Cocoa"
            "-framework IOKit"
            "-framework CoreFoundation"
    )
endif()