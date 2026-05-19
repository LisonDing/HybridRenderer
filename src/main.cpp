#include "Core/Application.h"
#include "Core/Logger.h"
#include <cstdlib>

int main() {
    // 1. 初始化核心日志系统
    Core::Logger::Init();
    HR_LOG_INFO("--- Hybrid Renderer Pro Boot ---");

    // 2. 实例化引擎并运行生命周期
    Core::Application app;
    app.Run();

    HR_LOG_INFO("--- System Shutdown gracefully ---");
    return EXIT_SUCCESS;
}