#pragma once

#include "stdafx.h"
#include "BridgeProtocol.h"

#include <cstdint>
#include <string>

class SocketTransfer {
public:
    SocketTransfer();
    ~SocketTransfer();

    static void open() {}
    static void close() {}
    void transactMessage(char* output, int outputSize, const char* input);

private:
    bool ensureConnected();
    bool connectSocket();
    void disconnect();
    bool sendFrame(tfar::bridge::Type type, std::uint32_t sequence, const std::string& payload);
    bool receiveFrame(tfar::bridge::Type& type, std::uint32_t& sequence, std::string& payload, int timeoutMs);
    void writeOutput(char* output, int outputSize, const std::string& text) const;
    std::string bridgeHost() const;
    std::uint16_t bridgePort() const;
    std::string bridgeToken() const;

    SOCKET socket_ = INVALID_SOCKET;
    std::uint32_t nextSequence_ = 1;
    bool winsockStarted_ = false;
};
