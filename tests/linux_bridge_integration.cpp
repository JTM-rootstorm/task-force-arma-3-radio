#include "../common/bridge/BridgeProtocol.hpp"
#include "../ts/src/transport/LinuxBridgeServer.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace {
int failures = 0;
#define CHECK(expression) do { if (!(expression)) { std::cerr << "FAIL line " << __LINE__ << ": " #expression "\n"; ++failures; } } while (false)

bool writeAll(int socket, const std::uint8_t* data, std::size_t length, std::size_t chunk = SIZE_MAX) {
    for (std::size_t offset = 0; offset < length;) {
        const auto count = std::min(chunk, length - offset);
        const auto sent = send(socket, data + offset, count, MSG_NOSIGNAL);
        if (sent <= 0) return false;
        offset += static_cast<std::size_t>(sent);
    }
    return true;
}

int connectClient(std::uint16_t port) {
    const int socketHandle = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) return -1;
    timeval timeout{ 1, 0 };
    setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    return socketHandle;
}

bool sendFrame(int socketHandle, tfar::bridge::Type type, std::uint32_t sequence, const std::string& payload, std::size_t chunk = SIZE_MAX) {
    const auto frame = tfar::bridge::encodeFrame(type, sequence, payload);
    return writeAll(socketHandle, frame.data(), frame.size(), chunk);
}

bool receiveFrame(int socketHandle, tfar::bridge::Type& type, std::uint32_t& sequence, std::string& payload) {
    std::array<std::uint8_t, tfar::bridge::kHeaderSize> bytes{};
    if (recv(socketHandle, bytes.data(), bytes.size(), MSG_WAITALL) != static_cast<ssize_t>(bytes.size())) return false;
    const auto header = tfar::bridge::decodeHeader(bytes.data());
    if (!tfar::bridge::isValidHeader(header)) return false;
    payload.assign(header.length, '\0');
    if (header.length && recv(socketHandle, payload.data(), payload.size(), MSG_WAITALL) != static_cast<ssize_t>(payload.size())) return false;
    type = static_cast<tfar::bridge::Type>(header.type);
    sequence = header.sequence;
    return true;
}

void testAuthenticationAndReconnect() {
    tfar::LinuxBridgeConfig config;
    config.port = 0;
    config.requireToken = true;
    config.token = "exact-token";
    tfar::LinuxBridgeServer server(config);
    CHECK(server.initialize());
    CHECK(server.boundPort() != 0);

    auto client = connectClient(server.boundPort());
    CHECK(client >= 0);
    CHECK(sendFrame(client, tfar::bridge::Type::SyncCommand, 1, "VERSION"));
    tfar::bridge::Type type{};
    std::uint32_t sequence = 0;
    std::string payload;
    CHECK(receiveFrame(client, type, sequence, payload));
    CHECK(type == tfar::bridge::Type::Error);
    CHECK(!server.isConnected());
    close(client);

    client = connectClient(server.boundPort());
    CHECK(sendFrame(client, tfar::bridge::Type::Hello, 0, "wrong"));
    CHECK(receiveFrame(client, type, sequence, payload));
    CHECK(type == tfar::bridge::Type::Error);
    close(client);

    client = connectClient(server.boundPort());
    CHECK(sendFrame(client, tfar::bridge::Type::Hello, 0, "exact-token", 3));
    CHECK(receiveFrame(client, type, sequence, payload));
    CHECK(type == tfar::bridge::Type::HelloAck);
    CHECK(payload.empty());
    CHECK(server.isConnected());
    CHECK(sendFrame(client, tfar::bridge::Type::SyncCommand, 7, "VERSION", 2));
    const auto command = server.receiveCommand(std::chrono::seconds(1));
    CHECK(command.has_value());
    CHECK(command->sequence == 7);
    CHECK(command->sessionGeneration != 0);
    CHECK(server.sendResponse(command->sequence, "OK", command->sessionGeneration));
    CHECK(receiveFrame(client, type, sequence, payload));
    CHECK(type == tfar::bridge::Type::Response);
    CHECK(sequence == 7);
    CHECK(payload == "OK");
    const auto oldGeneration = command->sessionGeneration;
    close(client);

    for (int attempt = 0; attempt < 50 && server.isConnected(); ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    client = connectClient(server.boundPort());
    CHECK(sendFrame(client, tfar::bridge::Type::Hello, 0, "exact-token"));
    CHECK(receiveFrame(client, type, sequence, payload));
    CHECK(!server.sendResponse(7, "STALE", oldGeneration));
    close(client);
    server.shutdown();
}

void testFailClosedConfiguration() {
    tfar::LinuxBridgeConfig config;
    config.host = "0.0.0.0";
    config.port = 0;
    tfar::LinuxBridgeServer external(config);
    CHECK(!external.initialize());

    config.host = "127.0.0.1";
    config.requireToken = true;
    tfar::LinuxBridgeServer missingToken(config);
    CHECK(!missingToken.initialize());
}
}

int main() {
    testAuthenticationAndReconnect();
    testFailClosedConfiguration();
    return failures == 0 ? 0 : 1;
}
