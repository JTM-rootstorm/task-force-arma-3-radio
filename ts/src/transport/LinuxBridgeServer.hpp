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
    bool allowExternalBridge = false;
    bool rejectCommandsWithoutConsumer = false;
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
    bool sendResponse(std::uint32_t sequence, const std::string& response, std::uint64_t sessionGeneration = 0) override;
    void setConfigNeedsRefresh(bool needsRefresh) override;
    std::uint16_t boundPort() const;

private:
    void run();
    enum class ClientState { Disconnected, AwaitingHello, Authenticated, Closing };
    enum class StartupResult { Pending, Listening, InvalidConfiguration, SocketFailed, BindFailed, ListenFailed };

    void closeClient(bool notifyDisconnect = true);
    void closeListenSocket();
    bool sendFrame(int socket, bridge::Type type, std::uint32_t sequence, const std::string& payload, std::uint64_t generation = 0);
    bool enqueue(GameCommand command);
    void clearSessionQueues();
    static bool isHighPriorityAsyncCommand(std::string_view command);
    bool authorizeHello(const std::string& payload) const;

    LinuxBridgeConfig config_;
    std::thread worker_;
    std::atomic_bool running_{ false };
    std::atomic_bool connected_{ false };
    std::atomic<std::uint64_t> activeGeneration_{ 0 };
    std::uint64_t nextGeneration_ = 0;
    mutable std::mutex socketMutex_;
    std::mutex sendMutex_;
    int listenSocket_ = -1;
    int clientSocket_ = -1;
    ClientState clientState_ = ClientState::Disconnected;
    std::atomic<std::uint16_t> boundPort_{ 0 };

    std::mutex startupMutex_;
    std::condition_variable startupCv_;
    StartupResult startupResult_ = StartupResult::Pending;

    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<GameCommand> syncQueue_;
    std::deque<GameCommand> highPriorityAsyncQueue_;
    std::deque<GameCommand> queue_;
    std::size_t highPriorityBudget_ = 8;
};

LinuxBridgeConfig loadLinuxBridgeConfigFromEnvironment();

} // namespace tfar

#endif
