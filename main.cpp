// Vulkan学习项目 — 程序入口点
// 创建FirstApp实例并启动主循环，捕获所有异常并记录调试信息

#include "first_app.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(){
    std::cerr << "[DEBUG main] Starting FirstApp construction..." << std::endl << std::flush;

    try {
        // 构建FirstApp（在构造函数中加载游戏对象和初始化资源）
        lve::FirstApp app{};
        std::cerr << "[DEBUG main] FirstApp constructed successfully" << std::endl << std::flush;

        // 运行主循环（包含初始化、子系统创建和渲染循环）
        std::cerr << "[DEBUG main] Calling app.run()..." << std::endl << std::flush;
        app.run();
        std::cerr << "[DEBUG main] app.run() returned normally" << std::endl << std::flush;
    } catch(const std::exception &e){
        std::cerr << "[DEBUG main] Exception caught: " << e.what() << std::endl << std::flush;
        return EXIT_FAILURE;
    } catch(...){
        std::cerr << "[DEBUG main] Unknown exception caught!" << std::endl << std::flush;
        return EXIT_FAILURE;
    }

    std::cerr << "[DEBUG main] Exiting successfully" << std::endl << std::flush;
    return EXIT_SUCCESS;
}
