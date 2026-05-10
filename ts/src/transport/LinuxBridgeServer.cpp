#include "LinuxBridgeServer.hpp"

#ifndef _WIN32

#include "BridgeProtocol.hpp"

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

void bridgeLog(const std::string& message) {
    std::fprintf(stderr, "%s\n", message.c_str());
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
            return false;
        }
        const ssize_t result = recv(socket, cursor + received, length - received, 0);
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
    if (end == value || port <= 0 || port > 65535) {
        return fallback;
    }
    return static_cast<std::uint16_t>(port);
}

} // namespace

namespace tfar {

LinuxBridgeConfig loadLinuxBridgeConfigFromEnvironment() {
    LinuxBridgeConfig config;
    config.port = readPortEnv(std::getenv("TFAR_BRIDGE_PORT"), config.port);
    if (const char* token = std::getenv("TFAR_BRIDGE_TOKEN")) {
        config.token = token;
    }
#ifdef TFAR_REQUIRE_BRIDGE_TOKEN
    config.requireToken = true;
#endif
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
    worker_ = std::thread(&LinuxBridgeServer::run, this);
    return true;
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
    if (!queueCv_.wait_for(lock, timeout, [this]() { return !queue_.empty() || !running_.load(); })) {
        return std::nullopt;
    }
    if (queue_.empty()) {
        return std::nullopt;
    }
    auto command = std::move(queue_.front());
    queue_.pop_front();
    return command;
}

bool LinuxBridgeServer::sendResponse(std::uint32_t sequence, const std::string& response) {
    std::lock_guard<std::mutex> lock(socketMutex_);
    if (clientSocket_ < 0) {
        return false;
    }
    return sendFrame(clientSocket_, bridge::Type::Response, sequence, response);
}

void LinuxBridgeServer::setConfigNeedsRefresh(bool needsRefresh) {
    configNeedsRefresh_.store(needsRefresh);
}

void LinuxBridgeServer::run() {
    listenSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket_ < 0) {
        bridgeLog("bridge: socket failed");
        return;
    }

    int one = 1;
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.host.c_str(), &address.sin_addr);

    if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        bridgeLog("bridge: bind failed on 127.0.0.1:" + std::to_string(config_.port));
        closeListenSocket();
        return;
    }
    if (listen(listenSocket_, 1) != 0) {
        bridgeLog("bridge: listen failed");
        closeListenSocket();
        return;
    }

    bridgeLog("bridge: listening on 127.0.0.1:" + std::to_string(config_.port));

    while (running_.load()) {
        pollfd descriptor{ listenSocket_, POLLIN, 0 };
        const int pollResult = poll(&descriptor, 1, 100);
        if (pollResult <= 0) {
            continue;
        }

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

        connected_.store(true);
        onConnected();
        bridgeLog("bridge: accepted connection");

        bool keepClient = true;
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
            switch (type) {
                case bridge::Type::Hello:
                    if (!authorizeHello(payload)) {
                        sendFrame(accepted, bridge::Type::Error, header.sequence, "unauthorized bridge token");
                        keepClient = false;
                    } else {
                        sendFrame(accepted, bridge::Type::HelloAck, header.sequence,
                                  "{\"server\":\"tfar-teamspeak-plugin\",\"protocol\":1,\"accepted\":true}");
                    }
                    break;
                case bridge::Type::Ping:
                    sendFrame(accepted, bridge::Type::Pong, header.sequence, payload);
                    break;
                case bridge::Type::SyncCommand:
                case bridge::Type::AsyncCommand:
                    enqueue(GameCommand{ std::move(payload), type == bridge::Type::AsyncCommand, header.sequence });
                    break;
                case bridge::Type::Disconnect:
                    keepClient = false;
                    break;
                default:
                    sendFrame(accepted, bridge::Type::Error, header.sequence, "unexpected bridge frame");
                    break;
            }
        }

        closeClient();
        connected_.store(false);
        onDisconnected();
        bridgeLog("bridge: disconnected");
    }
}

void LinuxBridgeServer::closeClient() {
    std::lock_guard<std::mutex> lock(socketMutex_);
    if (clientSocket_ >= 0) {
        close(clientSocket_);
        clientSocket_ = -1;
    }
}

void LinuxBridgeServer::closeListenSocket() {
    if (listenSocket_ >= 0) {
        close(listenSocket_);
        listenSocket_ = -1;
    }
}

bool LinuxBridgeServer::sendFrame(int socket, bridge::Type type, std::uint32_t sequence, const std::string& payload) {
    const auto frame = bridge::encodeFrame(type, sequence, payload);
    return !frame.empty() && writeExact(socket, frame.data(), frame.size());
}

void LinuxBridgeServer::enqueue(GameCommand command) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.push_back(std::move(command));
    }
    queueCv_.notify_one();
}

bool LinuxBridgeServer::authorizeHello(const std::string& payload) const {
    if (!config_.requireToken) {
        return true;
    }
    return bridge::containsToken(payload, config_.token);
}

} // namespace tfar

#endif
