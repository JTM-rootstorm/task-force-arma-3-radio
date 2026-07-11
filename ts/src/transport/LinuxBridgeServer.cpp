#include "LinuxBridgeServer.hpp"

#ifndef _WIN32

#include "BridgeProtocol.hpp"
#include "platform/Platform.hpp"

#include <cerrno>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <string_view>
#include <vector>
#include <chrono>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

void bridgeLog(const std::string& message) {
    tfar::platform::debugLog(message + "\n");
}

bool readExact(int socket, void* buffer, std::size_t length, std::atomic_bool& running) {
    auto* cursor = static_cast<std::uint8_t*>(buffer);
    std::size_t received = 0;
    while (received < length && running.load()) {
        pollfd descriptor{ socket, POLLIN, 0 };
        const int pollResult = poll(&descriptor, 1, 100);
        if (pollResult == 0) {
            continue;
        }
        if (pollResult < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) return false;
        const ssize_t result = recv(socket, cursor + received, length - received, 0);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) {
            return false;
        }
        received += static_cast<std::size_t>(result);
    }
    return received == length;
}

bool writeExact(int socket, const std::uint8_t* buffer, std::size_t length) {
    std::size_t sent = 0;
    while (sent < length) {
        const ssize_t result = send(socket, buffer + sent, length - sent, MSG_NOSIGNAL);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

std::uint16_t readPortEnv(const char* value, std::uint16_t fallback) {
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    char* end = nullptr;
    const long port = std::strtol(value, &end, 10);
    if (end == value || *end != '\0' || port <= 0 || port > 65535) {
        return fallback;
    }
    return static_cast<std::uint16_t>(port);
}

} // namespace

namespace tfar {
using namespace std::literals::string_view_literals;

namespace {
constexpr std::size_t kMaxLowPriorityCommandBacklog = 300;
constexpr std::size_t kMaxSyncCommandBacklog = 64;
constexpr std::size_t kMaxHighPriorityCommandBacklog = 256;

bool enabledEnvironmentValue(const char* value) {
    if (value == nullptr) return false;
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}
}

LinuxBridgeConfig loadLinuxBridgeConfigFromEnvironment() {
    LinuxBridgeConfig config;
    config.port = readPortEnv(std::getenv("TFAR_BRIDGE_PORT"), config.port);
    if (const char* host = std::getenv("TFAR_BRIDGE_HOST"); host != nullptr && *host != '\0') {
        config.host = host;
    }
    if (const char* token = std::getenv("TFAR_BRIDGE_TOKEN")) {
        config.token = token;
    }
    config.requireToken = enabledEnvironmentValue(std::getenv("TFAR_REQUIRE_BRIDGE_TOKEN"));
#ifdef TFAR_REQUIRE_BRIDGE_TOKEN
    if (std::getenv("TFAR_REQUIRE_BRIDGE_TOKEN") == nullptr) config.requireToken = true;
#endif
    config.allowExternalBridge = enabledEnvironmentValue(std::getenv("TFAR_ALLOW_EXTERNAL_BRIDGE"));
    return config;
}

LinuxBridgeServer::LinuxBridgeServer() : LinuxBridgeServer(loadLinuxBridgeConfigFromEnvironment()) {}

LinuxBridgeServer::LinuxBridgeServer(LinuxBridgeConfig config) : config_(std::move(config)) {}

LinuxBridgeServer::~LinuxBridgeServer() {
    shutdown();
}

bool LinuxBridgeServer::initialize() {
    if (running_.exchange(true)) {
        return true;
    }
    if (config_.requireToken && config_.token.empty()) {
        running_.store(false);
        bridgeLog("bridge: token authentication required but TFAR_BRIDGE_TOKEN is empty");
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(startupMutex_);
        startupResult_ = StartupResult::Pending;
    }
    worker_ = std::thread(&LinuxBridgeServer::run, this);
    std::unique_lock<std::mutex> lock(startupMutex_);
    const bool reported = startupCv_.wait_for(lock, std::chrono::seconds(2), [this]() {
        return startupResult_ != StartupResult::Pending;
    });
    const bool listening = reported && startupResult_ == StartupResult::Listening;
    lock.unlock();
    if (!listening) {
        running_.store(false);
        closeListenSocket();
        if (worker_.joinable()) worker_.join();
    }
    return listening;
}

void LinuxBridgeServer::shutdown() {
    if (!running_.exchange(false)) {
        return;
    }
    closeClient();
    closeListenSocket();
    queueCv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool LinuxBridgeServer::isConnected() const {
    return connected_.load();
}

std::optional<GameCommand> LinuxBridgeServer::receiveCommand(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(queueMutex_);
    if (!queueCv_.wait_for(lock, timeout, [this]() { return !syncQueue_.empty() || !highPriorityAsyncQueue_.empty() || !queue_.empty() || !running_.load(); })) {
        return std::nullopt;
    }
    if (!syncQueue_.empty()) {
        auto command = std::move(syncQueue_.front());
        syncQueue_.pop_front();
        return command;
    }
    if (!highPriorityAsyncQueue_.empty() && highPriorityBudget_ > 0) {
        auto command = std::move(highPriorityAsyncQueue_.front());
        highPriorityAsyncQueue_.pop_front();
        --highPriorityBudget_;
        return command;
    }
    if (!queue_.empty()) {
        auto command = std::move(queue_.front());
        queue_.pop_front();
        highPriorityBudget_ = 8;
        return command;
    }
    if (!highPriorityAsyncQueue_.empty()) {
        auto command = std::move(highPriorityAsyncQueue_.front());
        highPriorityAsyncQueue_.pop_front();
        highPriorityBudget_ = 8;
        return command;
    }
    return std::nullopt;
}

bool LinuxBridgeServer::sendResponse(std::uint32_t sequence, const std::string& response, std::uint64_t generation) {
    int socket = -1;
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        if (clientSocket_ < 0 || clientState_ != ClientState::Authenticated || generation != activeGeneration_.load()) return false;
        socket = clientSocket_;
    }
    if (!sendFrame(socket, bridge::Type::Response, sequence, response, generation)) {
        closeClient();
        return false;
    }
    return true;
}

void LinuxBridgeServer::setConfigNeedsRefresh(bool) {
}

std::uint16_t LinuxBridgeServer::boundPort() const { return boundPort_.load(); }

void LinuxBridgeServer::run() {
    auto reportStartup = [this](StartupResult result) {
        {
            std::lock_guard<std::mutex> lock(startupMutex_);
            startupResult_ = result;
        }
        startupCv_.notify_all();
    };

    in_addr parsedAddress{};
    if (inet_pton(AF_INET, config_.host.c_str(), &parsedAddress) != 1 ||
        (!config_.allowExternalBridge && (ntohl(parsedAddress.s_addr) >> 24u) != 127u)) {
        bridgeLog("bridge: refusing invalid or non-loopback host " + config_.host);
        reportStartup(StartupResult::InvalidConfiguration);
        return;
    }

    listenSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket_ < 0) {
        bridgeLog("bridge: socket failed");
        reportStartup(StartupResult::SocketFailed);
        return;
    }

    int one = 1;
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(config_.port);
    address.sin_addr = parsedAddress;

    if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        bridgeLog("bridge: bind failed on " + config_.host + ":" + std::to_string(config_.port));
        reportStartup(StartupResult::BindFailed);
        closeListenSocket();
        return;
    }
    if (listen(listenSocket_, 1) != 0) {
        bridgeLog("bridge: listen failed");
        reportStartup(StartupResult::ListenFailed);
        closeListenSocket();
        return;
    }

    sockaddr_in bound{};
    socklen_t boundLength = sizeof(bound);
    if (getsockname(listenSocket_, reinterpret_cast<sockaddr*>(&bound), &boundLength) == 0) boundPort_.store(ntohs(bound.sin_port));
    reportStartup(StartupResult::Listening);
    bridgeLog("bridge: listening on " + config_.host + ":" + std::to_string(boundPort_.load()));

    while (running_.load()) {
        pollfd descriptor{ listenSocket_, POLLIN, 0 };
        const int pollResult = poll(&descriptor, 1, 100);
        if (pollResult < 0 && errno == EINTR) continue;
        if (pollResult <= 0) {
            continue;
        }
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) break;

        int accepted = accept(listenSocket_, nullptr, nullptr);
        if (accepted < 0) {
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(socketMutex_);
            if (clientSocket_ >= 0) {
                close(clientSocket_);
            }
            clientSocket_ = accepted;
        }

        bridgeLog("bridge: accepted connection");

        {
            std::lock_guard<std::mutex> lock(socketMutex_);
            clientState_ = ClientState::AwaitingHello;
        }

        bool keepClient = true;
        bool authenticated = false;
        while (running_.load() && keepClient) {
            std::array<std::uint8_t, bridge::kHeaderSize> encodedHeader{};
            if (!readExact(accepted, encodedHeader.data(), encodedHeader.size(), running_)) {
                break;
            }

            const auto header = bridge::decodeHeader(encodedHeader.data());
            if (!bridge::isValidHeader(header)) {
                sendFrame(accepted, bridge::Type::Error, header.sequence, "bad bridge header");
                break;
            }

            std::string payload(header.length, '\0');
            if (header.length > 0 && !readExact(accepted, payload.data(), payload.size(), running_)) {
                break;
            }

            const auto type = static_cast<bridge::Type>(header.type);
            if (!authenticated && type != bridge::Type::Hello) {
                sendFrame(accepted, bridge::Type::Error, header.sequence, "hello required before commands");
                break;
            }
            switch (type) {
                case bridge::Type::Hello:
                    if (authenticated) {
                        sendFrame(accepted, bridge::Type::Error, header.sequence, "duplicate hello");
                        keepClient = false;
                    } else if (header.sequence != 0) {
                        sendFrame(accepted, bridge::Type::Error, header.sequence, "hello sequence must be zero");
                        keepClient = false;
                    } else if (!authorizeHello(payload)) {
                        sendFrame(accepted, bridge::Type::Error, header.sequence, "unauthorized bridge token");
                        keepClient = false;
                    } else {
                        const auto generation = ++nextGeneration_;
                        activeGeneration_.store(generation);
                        {
                            std::lock_guard<std::mutex> lock(socketMutex_);
                            clientState_ = ClientState::Authenticated;
                        }
                        authenticated = true;
                        connected_.store(true);
                        sendFrame(accepted, bridge::Type::HelloAck, 0, {}, generation);
                        onConnected();
                    }
                    break;
                case bridge::Type::Ping:
                    if (header.sequence != 0) { keepClient = false; break; }
                    sendFrame(accepted, bridge::Type::Pong, header.sequence, payload);
                    break;
                case bridge::Type::SyncCommand:
                    if (header.sequence == 0) { sendFrame(accepted, bridge::Type::Error, 0, "sync sequence must be nonzero"); break; }
                    if (config_.rejectCommandsWithoutConsumer) {
                        sendFrame(accepted, bridge::Type::Error, header.sequence, "bridge command processor unavailable");
                        break;
                    }
                    if (!enqueue(GameCommand{ std::move(payload), false, header.sequence, activeGeneration_.load() }))
                        sendFrame(accepted, bridge::Type::Error, header.sequence, "bridge sync queue full", activeGeneration_.load());
                    break;
                case bridge::Type::AsyncCommand:
                    if (header.sequence != 0) { sendFrame(accepted, bridge::Type::Error, header.sequence, "async sequence must be zero"); break; }
                    if (config_.rejectCommandsWithoutConsumer) {
                        break;
                    }
                    enqueue(GameCommand{ std::move(payload), true, header.sequence, activeGeneration_.load() });
                    break;
                case bridge::Type::Disconnect:
                    keepClient = false;
                    break;
                default:
                    sendFrame(accepted, bridge::Type::Error, header.sequence, "unexpected bridge frame");
                    break;
            }
        }

        closeClient(authenticated);
        bridgeLog("bridge: disconnected");
    }
}

void LinuxBridgeServer::closeClient(bool notifyDisconnect) {
    bool wasAuthenticated = false;
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        wasAuthenticated = clientState_ == ClientState::Authenticated;
        clientState_ = ClientState::Closing;
        if (clientSocket_ >= 0) {
            ::shutdown(clientSocket_, SHUT_RDWR);
            close(clientSocket_);
            clientSocket_ = -1;
        }
        clientState_ = ClientState::Disconnected;
        connected_.store(false);
        activeGeneration_.store(0);
    }
    clearSessionQueues();
    if (notifyDisconnect && wasAuthenticated) onDisconnected();
}

void LinuxBridgeServer::closeListenSocket() {
    if (listenSocket_ >= 0) {
        close(listenSocket_);
        listenSocket_ = -1;
    }
}

bool LinuxBridgeServer::sendFrame(int socket, bridge::Type type, std::uint32_t sequence, const std::string& payload, std::uint64_t generation) {
    const auto frame = bridge::encodeFrame(type, sequence, payload);
    if (frame.empty()) return false;
    std::lock_guard<std::mutex> sendLock(sendMutex_);
    if (generation != 0) {
        std::lock_guard<std::mutex> socketLock(socketMutex_);
        if (socket != clientSocket_ || generation != activeGeneration_.load()) return false;
    }
    return writeExact(socket, frame.data(), frame.size());
}

bool LinuxBridgeServer::enqueue(GameCommand command) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (!command.async) {
            if (syncQueue_.size() >= kMaxSyncCommandBacklog) return false;
            syncQueue_.push_back(std::move(command));
        } else if (isHighPriorityAsyncCommand(command.payload)) {
            if (highPriorityAsyncQueue_.size() >= kMaxHighPriorityCommandBacklog) return false;
            highPriorityAsyncQueue_.push_back(std::move(command));
        } else {
            if (queue_.size() >= kMaxLowPriorityCommandBacklog) {
                queue_.pop_front();
            }
            queue_.push_back(std::move(command));
        }
    }
    queueCv_.notify_one();
    return true;
}

void LinuxBridgeServer::clearSessionQueues() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    syncQueue_.clear();
    highPriorityAsyncQueue_.clear();
    queue_.clear();
    highPriorityBudget_ = 8;
    queueCv_.notify_all();
}

bool LinuxBridgeServer::isHighPriorityAsyncCommand(std::string_view command) {
    const auto commandEnd = command.find('\t');
    const auto commandName = command.substr(0, commandEnd);
    return commandName != "POS"sv && commandName != "TRACK"sv && commandName != "collectDebugInfo"sv;
}

bool LinuxBridgeServer::authorizeHello(const std::string& payload) const {
    return !config_.requireToken || payload == config_.token;
}

} // namespace tfar

#endif
