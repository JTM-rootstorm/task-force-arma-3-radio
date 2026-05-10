#include "stdafx.h"
#include "SocketTransfer.h"
#include "BridgeProtocol.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

namespace {

std::string getenvString(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return {};
    }
    return value;
}

bool recvExact(SOCKET socket, void* data, int length) {
    char* cursor = static_cast<char*>(data);
    int received = 0;
    while (received < length) {
        const int result = recv(socket, cursor + received, length - received, 0);
        if (result <= 0) {
            return false;
        }
        received += result;
    }
    return true;
}

bool sendExact(SOCKET socket, const std::uint8_t* data, int length) {
    int sent = 0;
    while (sent < length) {
        const int result = send(socket, reinterpret_cast<const char*>(data) + sent, length - sent, 0);
        if (result <= 0) {
            return false;
        }
        sent += result;
    }
    return true;
}

std::uint16_t parsePort(const std::string& text, std::uint16_t fallback) {
    if (text.empty()) {
        return fallback;
    }
    char* end = nullptr;
    const long port = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || port <= 0 || port > 65535) {
        return fallback;
    }
    return static_cast<std::uint16_t>(port);
}

} // namespace

SocketTransfer::SocketTransfer() {
    WSADATA data{};
    winsockStarted_ = WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

SocketTransfer::~SocketTransfer() {
    disconnect();
    if (winsockStarted_) {
        WSACleanup();
    }
}

void SocketTransfer::transactMessage(char* output, int outputSize, const char* input) {
    if (outputSize <= 0) {
        return;
    }

    std::string command(input);
    const bool async = !command.empty() && command.back() == '~';
    if (async) {
        command.pop_back();
    }

    if (!ensureConnected()) {
        writeOutput(output, outputSize, "Not connected to TeamSpeak");
        return;
    }

    const std::uint32_t sequence = async ? 0 : nextSequence_++;
    if (!sendFrame(async ? tfar::bridge::Type::AsyncCommand : tfar::bridge::Type::SyncCommand, sequence, command)) {
        disconnect();
        writeOutput(output, outputSize, "Not connected to TeamSpeak");
        return;
    }

    if (async) {
        if (!command.empty() && command.front() == 'D') {
            writeOutput(output, outputSize, "OK");
        } else if (!command.empty() && command.front() == 'M') {
            disconnect();
            writeOutput(output, outputSize, "OK");
        } else {
            writeOutput(output, outputSize, "OK");
        }
        return;
    }

    tfar::bridge::Type type{};
    std::uint32_t responseSequence = 0;
    std::string payload;
    if (!receiveFrame(type, responseSequence, payload, PIPE_TIMEOUT) ||
        responseSequence != sequence ||
        (type != tfar::bridge::Type::Response && type != tfar::bridge::Type::Error)) {
        disconnect();
        writeOutput(output, outputSize, "Not connected to TeamSpeak");
        return;
    }

    writeOutput(output, outputSize, payload);
}

bool SocketTransfer::ensureConnected() {
    if (socket_ != INVALID_SOCKET) {
        return true;
    }
    return connectSocket();
}

bool SocketTransfer::connectSocket() {
    if (!winsockStarted_) {
        return false;
    }

    SOCKET socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
    if (socket == INVALID_SOCKET) {
        return false;
    }

    DWORD timeout = PIPE_TIMEOUT;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(bridgePort());
    if (InetPtonA(AF_INET, bridgeHost().c_str(), &address.sin_addr) != 1) {
        closesocket(socket);
        return false;
    }

    if (connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        closesocket(socket);
        return false;
    }

    socket_ = socket;
    const std::string token = bridgeToken();
    const std::string hello = std::string("{\"client\":\"tfar-arma-extension\",\"protocol\":1,\"token\":\"") + token + "\"}";
    if (!sendFrame(tfar::bridge::Type::Hello, 0, hello)) {
        disconnect();
        return false;
    }

    tfar::bridge::Type type{};
    std::uint32_t sequence = 0;
    std::string payload;
    if (!receiveFrame(type, sequence, payload, PIPE_TIMEOUT) || type != tfar::bridge::Type::HelloAck) {
        disconnect();
        return false;
    }

    return true;
}

void SocketTransfer::disconnect() {
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
}

bool SocketTransfer::sendFrame(tfar::bridge::Type type, std::uint32_t sequence, const std::string& payload) {
    const auto frame = tfar::bridge::encodeFrame(type, sequence, payload);
    return !frame.empty() && sendExact(socket_, frame.data(), static_cast<int>(frame.size()));
}

bool SocketTransfer::receiveFrame(tfar::bridge::Type& type, std::uint32_t& sequence, std::string& payload, int timeoutMs) {
    DWORD timeout = static_cast<DWORD>(timeoutMs);
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    std::array<std::uint8_t, tfar::bridge::kHeaderSize> encodedHeader{};
    if (!recvExact(socket_, encodedHeader.data(), static_cast<int>(encodedHeader.size()))) {
        return false;
    }

    const auto header = tfar::bridge::decodeHeader(encodedHeader.data());
    if (!tfar::bridge::isValidHeader(header)) {
        return false;
    }

    payload.assign(header.length, '\0');
    if (header.length > 0 && !recvExact(socket_, &payload[0], static_cast<int>(payload.size()))) {
        return false;
    }

    type = static_cast<tfar::bridge::Type>(header.type);
    sequence = header.sequence;
    return true;
}

void SocketTransfer::writeOutput(char* output, int outputSize, const std::string& text) const {
    if (outputSize <= 0) {
        return;
    }
    const auto count = std::min<std::size_t>(text.size(), static_cast<std::size_t>(outputSize - 1));
    std::memcpy(output, text.data(), count);
    output[count] = '\0';
}

std::string SocketTransfer::bridgeHost() const {
    const auto host = getenvString("TFAR_BRIDGE_HOST");
    return host.empty() ? "127.0.0.1" : host;
}

std::uint16_t SocketTransfer::bridgePort() const {
    return parsePort(getenvString("TFAR_BRIDGE_PORT"), static_cast<std::uint16_t>(tfar::bridge::kDefaultPort));
}

std::string SocketTransfer::bridgeToken() const {
    return getenvString("TFAR_BRIDGE_TOKEN");
}

#ifdef TFAR_USE_SOCKET_BRIDGE
SocketTransfer transfer;
#endif
