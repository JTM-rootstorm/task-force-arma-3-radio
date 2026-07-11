#pragma once

#include "stdafx.h"
#include "BridgeProtocol.h"

#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

class SocketTransfer {
public:
    SocketTransfer();
    ~SocketTransfer();
    void shutdown();

    static void open() {}
    static void close() {}
    void transactMessage(char* output, int outputSize, const char* input);

private:
    bool ensureConnected();
    bool ensureConnectedLocked();
    bool connectSocketLocked();
    void disconnect();
    void disconnectLocked();
    bool sendFrame(tfar::bridge::Type type, std::uint32_t sequence, const std::string& payload);
    bool receiveFrame(tfar::bridge::Type& type, std::uint32_t& sequence, std::string& payload, int timeoutMs);
    void queueAsyncCommand(std::string command);
    void queueCachedSyncRefresh(std::string command);
    void ensureAsyncWorker();
    void stopAsyncWorker();
    void asyncWorkerLoop();
    bool sendSyncCommandLocked(const std::string& command, std::string& response);
    static bool isHighPriorityAsyncCommand(const std::string& command);
    static bool isCachedSyncCommand(const std::string& command);
    static bool isCachedSpeakingCommand(const std::string& command);
    static std::string defaultCachedSyncResponse(const std::string& command);
    std::string cachedSyncResponse(const std::string& command);
    void writeOutput(char* output, int outputSize, const std::string& text) const;
    std::string bridgeHost() const;
    std::uint16_t bridgePort() const;
    std::string bridgeToken() const;

    std::mutex socketMutex_;
    SOCKET socket_ = INVALID_SOCKET;
    std::uint32_t nextSequence_ = 1;
    bool winsockStarted_ = false;
	std::atomic_bool connected_{ false };

    std::mutex asyncMutex_;
    std::condition_variable asyncCv_;
    std::deque<std::string> cachedSyncQueue_;
    std::unordered_set<std::string> queuedCachedSyncCommands_;
    std::deque<std::string> highPriorityAsyncQueue_;
    std::deque<std::string> asyncQueue_;
    std::thread asyncThread_;
    bool asyncWorkerStarted_ = false;
    bool stopAsyncWorker_ = false;
	bool reconnectRequested_ = false;

    std::mutex cachedSyncMutex_;
    std::unordered_map<std::string, std::string> cachedSyncResponses_;
};
