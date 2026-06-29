#include "kvstore/store.hpp"
#include "kvstore/server.hpp"
#include <spdlog/spdlog.h>
#include <csignal>
#include <iostream>

using namespace kvstore;

static std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    spdlog::info("Received signal {}, shutting down...", signum);
    g_running = false;
}

int main(int argc, char* argv[]) {
    // Parse command line
    uint16_t port = 6379;       // Default Redis port
    std::string dataDir = "./kvdata";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "--dir" || arg == "-d") && i + 1 < argc) {
            dataDir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: kvstore [options]\n"
                      << "  -p, --port PORT   Listen port (default: 6379)\n"
                      << "  -d, --dir  DIR    Data directory (default: ./kvdata)\n"
                      << "  -h, --help        Show this help\n";
            return 0;
        }
    }
    
    // Setup logging
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    
    // Signal handling
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Create and open store
    StoreConfig config{
        .dataDir = dataDir,
        .ttlScanInterval = std::chrono::seconds(1),
        .snapshotInterval = std::chrono::minutes(5),
        .enablePersistence = true,
    };
    
    KVStore store(config);
    auto result = store.open();
    if (!result) {
        spdlog::error("Failed to open store: {}", static_cast<int>(result.error()));
        return 1;
    }
    
    // Run server
    Server server(store, port);
    
    spdlog::info("Starting KVStore server on port {}...", port);
    spdlog::info("Data directory: {}", dataDir);
    spdlog::info("Use: redis-cli -p {} to connect", port);
    
    server.run();  // Blocks until stopped
    
    store.close();
    return 0;
}
