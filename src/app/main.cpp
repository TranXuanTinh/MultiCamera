#include "app/edge_ai_manager.h"
#include <iostream>
#include <csignal>
#include <string>

int main(int argc, char* argv[]) {
    std::string config_path = "config/app_config.yaml";
    bool use_mock_hal = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--mock" || arg == "-m") {
            use_mock_hal = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: edge-ai-manager [options]\n"
                      << "Options:\n"
                      << "  -c, --config <file>  Path to YAML configuration file (default: config/app_config.yaml)\n"
                      << "  -m, --mock           Run in host PC mock mode (without Jetson GPU/CSI camera)\n"
                      << "  -h, --help           Show this help message\n"
                      << "  -v, --version        Print version info\n";
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "edge-ai-manager v1.0.0 (Jetson Nano / Yocto Linux / C++17)\n";
            return 0;
        }
    }

    // Install POSIX signal handlers
    std::signal(SIGINT,  edge_ai::EdgeAIManager::signalHandler);
    std::signal(SIGTERM, edge_ai::EdgeAIManager::signalHandler);

    edge_ai::EdgeAIManager app;

    if (!app.initialize(config_path, use_mock_hal)) {
        std::cerr << "[Fatal Error] EdgeAIManager failed to initialize.\n";
        return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}

