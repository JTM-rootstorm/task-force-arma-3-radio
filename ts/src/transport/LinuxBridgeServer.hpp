#pragma once

#include "BridgeProtocol.hpp"
#include "IGameTransport.hpp"

#ifndef _WIN32

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace tfar {

struct LinuxBridgeConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 47333;
    std::string token;
    bool requireToken = false;
};

class LinuxBridgeServer final : public IGameTransport {
public:
    LinuxBridgeServer();
    explicit LinuxBridgeServer(LinuxBridgeConfig config);
    ~LinuxBridgeServer() override;

    bool initialize() override;
    void shutdown() override;
    bool isConnected() const override;
    std::optional<GameCommand> receiveCommand(std::chrono::milliseconds timeout) override;
    bool sendResponse(std::uint32_t sequence, const std::string& response) override;
    void setConfigNeedsRefresh(bool needsRefresh) override;

private:
    void run();
    void closeClient();
    void closeListenSocket();
    bool sendFrame(int socket, bridge::Type type, std::uint32_t sequence, const std::string& payload);
    void enqueue(GameCommand command);
    bool authorizeHello(const std::string& payload) const;

    LinuxBridgeConfig config_;
    std::thread worker_;
    std::atomic_bool running_{ false };
    std::atomic_bool connected_{ false };
    std::atomic_bool configNeedsRefresh_{ false };
    mutable std::mutex socketMutex_;
    int listenSocket_ = -1;
    int clientSocket_ = -1;

    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<GameCommand> queue_;
};

LinuxBridgeConfig loadLinuxBridgeConfigFromEnvironment();

} // namespace tfar

#endif
