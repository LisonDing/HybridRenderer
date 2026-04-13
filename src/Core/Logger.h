#pragma once

#include <iostream>
#include <string>

// 现代 C++ 引擎通常会封装一套宏，方便后续扩展（比如加入文件名、行号，或改变终端颜色）
// 这里我们先做一个极简的占位版本，保持接口规范
namespace Core {

class Logger {
public:
    static void Init() {
        // 后续可以在这里初始化更复杂的第三方日志库（如 spdlog）
        std::cout << "[Logger] System Initialized.\n";
    }

    static void Info(const std::string& message) {
        std::cout << "[INFO] " << message << "\n";
    }

    static void Error(const std::string& message) {
        std::cerr << "[ERROR] " << message << "\n";
    }
};

} // namespace Core

// 提供宏定义，这是 C++ 引擎的常规操作，方便调用且能在 Release 模式下被一键剥离
#define HR_LOG_INFO(...)  ::Core::Logger::Info(__VA_ARGS__)
#define HR_LOG_ERROR(...) ::Core::Logger::Error(__VA_ARGS__)