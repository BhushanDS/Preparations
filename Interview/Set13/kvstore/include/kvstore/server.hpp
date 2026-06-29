#pragma once

#include "kvstore/store.hpp"
#include "kvstore/protocol.hpp"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <spdlog/spdlog.h>

namespace kvstore {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class Server {
public:
    Server(KVStore& store, uint16_t port, unsigned threads = 0)
        : store_(store),
          port_(port),
          numThreads_(threads > 0 ? threads : std::thread::hardware_concurrency()),
          io_(static_cast<int>(numThreads_)) {}
    
    // Start the server (blocking - runs the event loop)
    void run() {
        tcp::acceptor acceptor(io_, {tcp::v4(), port_});
        acceptor.set_option(tcp::acceptor::reuse_address(true));
        
        spdlog::info("KVStore server listening on port {} ({} threads)", 
                     port_, numThreads_);
        
        asio::co_spawn(io_, accept(std::move(acceptor)), asio::detached);
        
        // Run on multiple threads
        std::vector<std::jthread> threads;
        for (unsigned i = 1; i < numThreads_; ++i) {
            threads.emplace_back([this] { io_.run(); });
        }
        io_.run();  // Main thread
    }
    
    void stop() {
        io_.stop();
        spdlog::info("Server stopped");
    }
    
private:
    KVStore& store_;
    uint16_t port_;
    unsigned numThreads_;
    asio::io_context io_;
    
    // Accept loop coroutine
    asio::awaitable<void> accept(tcp::acceptor acceptor) {
        for (;;) {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            auto endpoint = socket.remote_endpoint();
            spdlog::debug("Client connected: {}:{}", 
                         endpoint.address().to_string(), endpoint.port());
            
            auto executor = co_await asio::this_coro::executor;
            asio::co_spawn(executor, 
                           handleClient(std::move(socket)), 
                           asio::detached);
        }
    }
    
    // Per-client connection handler
    asio::awaitable<void> handleClient(tcp::socket socket) {
        try {
            asio::streambuf buf;
            
            for (;;) {
                // Read one line (command)
                auto bytes = co_await asio::async_read_until(
                    socket, buf, "\n", asio::use_awaitable);
                
                // Extract the line
                std::istream stream(&buf);
                std::string line;
                std::getline(stream, line);
                
                // Parse and execute
                auto cmd = Protocol::parse(line);
                
                if (cmd.type == CommandType::QUIT) {
                    auto response = Protocol::ok();
                    co_await asio::async_write(
                        socket, asio::buffer(response), asio::use_awaitable);
                    break;
                }
                
                auto response = store_.execute(cmd);
                
                co_await asio::async_write(
                    socket, asio::buffer(response), asio::use_awaitable);
            }
        } catch (const boost::system::system_error& e) {
            if (e.code() != asio::error::eof && 
                e.code() != asio::error::connection_reset) {
                spdlog::warn("Client error: {}", e.what());
            }
        }
        spdlog::debug("Client disconnected");
    }
};

} // namespace kvstore
