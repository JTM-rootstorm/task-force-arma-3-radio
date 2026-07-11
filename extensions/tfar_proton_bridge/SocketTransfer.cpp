#include "stdafx.h"
#include "SocketTransfer.h"
#include "BridgeProtocol.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

namespace {
constexpr std::size_t kMaxHighPriorityAsyncBacklog = 300;
constexpr std::size_t kMaxLowPriorityAsyncBacklog = 60;
constexpr std::size_t kMaxCachedSyncBacklog = 8;

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
    if (end == text.c_str() || *end != '\0' || port <= 0 || port > 65535) {
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
	shutdown();
	if (winsockStarted_) WSACleanup();
}

void SocketTransfer::shutdown() {
    stopAsyncWorker();
    disconnect();
	std::lock_guard<std::mutex> lock(cachedSyncMutex_);
	cachedSyncResponses_.clear();
}

void SocketTransfer::transactMessage(char* output, int outputSize, const char* input) {
    if (outputSize <= 0) {
        return;
    }

    std::string command(input);
    const bool asyncMarker = !command.empty() && command.back() == '~';
    if (asyncMarker) {
        command.pop_back();
    }
    const bool needsSynchronousAnswer = command == "DFRAME";
    const bool async = asyncMarker && !needsSynchronousAnswer;

    if (!async && isCachedSyncCommand(command)) {
        queueCachedSyncRefresh(command);
        writeOutput(output, outputSize, cachedSyncResponse(command));
        return;
    }

    if (async) {
        const bool isMissionEndCommand = command == "MISSIONEND";
		if (!connected_.load()) {
			ensureAsyncWorker();
			{
				std::lock_guard<std::mutex> lock(asyncMutex_);
				reconnectRequested_ = true;
			}
			asyncCv_.notify_one();
            writeOutput(output, outputSize, "Not connected to TeamSpeak");
            return;
        }
        queueAsyncCommand(std::move(command));
        writeOutput(output, outputSize, "OK");
        return;
    }

    std::string payload;
    {
        std::lock_guard<std::mutex> socketLock(socketMutex_);
        if (!ensureConnectedLocked()) {
            writeOutput(output, outputSize, "Not connected to TeamSpeak");
            return;
        }

        if (!sendSyncCommandLocked(command, payload)) {
            disconnectLocked();
            writeOutput(output, outputSize, "Not connected to TeamSpeak");
            return;
        }
    }

    if (command == "MISSIONEND") {
        disconnect();
    }

    writeOutput(output, outputSize, payload);
}

bool SocketTransfer::ensureConnected() {
    std::lock_guard<std::mutex> socketLock(socketMutex_);
    return ensureConnectedLocked();
}

bool SocketTransfer::ensureConnectedLocked() {
    if (socket_ != INVALID_SOCKET) {
        return true;
    }
    return connectSocketLocked();
}

bool SocketTransfer::connectSocketLocked() {
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
    if (!sendFrame(tfar::bridge::Type::Hello, 0, token)) {
        disconnectLocked();
        return false;
    }

    tfar::bridge::Type type{};
    std::uint32_t sequence = 0;
    std::string payload;
    if (!receiveFrame(type, sequence, payload, PIPE_TIMEOUT) || type != tfar::bridge::Type::HelloAck) {
        disconnectLocked();
        return false;
    }

	connected_.store(true);
    return true;
}

void SocketTransfer::disconnect() {
    std::lock_guard<std::mutex> socketLock(socketMutex_);
    disconnectLocked();
}

void SocketTransfer::disconnectLocked() {
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
	connected_.store(false);
	nextSequence_ = 1;
	std::lock_guard<std::mutex> cacheLock(cachedSyncMutex_);
	cachedSyncResponses_.clear();
}

void SocketTransfer::queueAsyncCommand(std::string command) {
    ensureAsyncWorker();
    {
        std::lock_guard<std::mutex> lock(asyncMutex_);
        auto& queue = isHighPriorityAsyncCommand(command) ? highPriorityAsyncQueue_ : asyncQueue_;
        const auto maxBacklog = isHighPriorityAsyncCommand(command) ? kMaxHighPriorityAsyncBacklog : kMaxLowPriorityAsyncBacklog;
        if (queue.size() >= maxBacklog) {
            queue.pop_front();
        }
        queue.emplace_back(std::move(command));
    }
    asyncCv_.notify_one();
}

void SocketTransfer::queueCachedSyncRefresh(std::string command) {
    ensureAsyncWorker();
    {
        std::lock_guard<std::mutex> lock(asyncMutex_);
        if (!queuedCachedSyncCommands_.emplace(command).second) {
            return;
        }
        if (cachedSyncQueue_.size() >= kMaxCachedSyncBacklog) {
            queuedCachedSyncCommands_.erase(cachedSyncQueue_.front());
            cachedSyncQueue_.pop_front();
        }
        cachedSyncQueue_.emplace_back(std::move(command));
    }
    asyncCv_.notify_one();
}

void SocketTransfer::ensureAsyncWorker() {
    std::lock_guard<std::mutex> lock(asyncMutex_);
    if (asyncWorkerStarted_) {
        return;
    }
        stopAsyncWorker_ = false;
		reconnectRequested_ = false;
    asyncThread_ = std::thread(&SocketTransfer::asyncWorkerLoop, this);
    asyncWorkerStarted_ = true;
}

void SocketTransfer::stopAsyncWorker() {
    {
        std::lock_guard<std::mutex> lock(asyncMutex_);
        if (!asyncWorkerStarted_) {
            return;
        }
        stopAsyncWorker_ = true;
    }
    asyncCv_.notify_one();
    if (asyncThread_.joinable()) {
        asyncThread_.join();
    }
    {
        std::lock_guard<std::mutex> lock(asyncMutex_);
        asyncWorkerStarted_ = false;
        cachedSyncQueue_.clear();
        queuedCachedSyncCommands_.clear();
        highPriorityAsyncQueue_.clear();
        asyncQueue_.clear();
    }
}

void SocketTransfer::asyncWorkerLoop() {
    while (true) {
        std::string command;
        {
            std::unique_lock<std::mutex> lock(asyncMutex_);
            asyncCv_.wait(lock, [this]() {
                return stopAsyncWorker_ || reconnectRequested_ || !highPriorityAsyncQueue_.empty() || !cachedSyncQueue_.empty() || !asyncQueue_.empty();
            });
            if (stopAsyncWorker_) {
                return;
            }
			if (reconnectRequested_ && highPriorityAsyncQueue_.empty() && cachedSyncQueue_.empty() && asyncQueue_.empty()) {
				reconnectRequested_ = false;
				lock.unlock();
				std::lock_guard<std::mutex> socketLock(socketMutex_);
				ensureConnectedLocked();
				continue;
			}
            if (!highPriorityAsyncQueue_.empty()) {
                command = std::move(highPriorityAsyncQueue_.front());
                highPriorityAsyncQueue_.pop_front();
            } else if (!cachedSyncQueue_.empty()) {
                command = std::move(cachedSyncQueue_.front());
                cachedSyncQueue_.pop_front();
                queuedCachedSyncCommands_.erase(command);
            } else {
                command = std::move(asyncQueue_.front());
                asyncQueue_.pop_front();
            }
        }

        if (isCachedSyncCommand(command)) {
            std::string response;
            std::lock_guard<std::mutex> socketLock(socketMutex_);
            if (ensureConnectedLocked() && sendSyncCommandLocked(command, response)) {
                std::lock_guard<std::mutex> cacheLock(cachedSyncMutex_);
                cachedSyncResponses_[command] = std::move(response);
            } else {
                disconnectLocked();
            }
            continue;
        }

        {
            std::lock_guard<std::mutex> socketLock(socketMutex_);
            if (!ensureConnectedLocked() ||
                !sendFrame(tfar::bridge::Type::AsyncCommand, 0, command)) {
                disconnectLocked();
            }
        }

        if (command == "MISSIONEND") {
            disconnect();
        }
        if (!isHighPriorityAsyncCommand(command)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

bool SocketTransfer::sendSyncCommandLocked(const std::string& command, std::string& response) {
    const std::uint32_t sequence = nextSequence_++;
	if (nextSequence_ == 0) nextSequence_ = 1;
    tfar::bridge::Type type{};
    std::uint32_t responseSequence = 0;
    return sendFrame(tfar::bridge::Type::SyncCommand, sequence, command) &&
        receiveFrame(type, responseSequence, response, PIPE_TIMEOUT) &&
        responseSequence == sequence &&
        (type == tfar::bridge::Type::Response || type == tfar::bridge::Type::Error);
}

bool SocketTransfer::isHighPriorityAsyncCommand(const std::string& command) {
    const auto commandEnd = command.find('\t');
    const auto commandName = command.substr(0, commandEnd);
    return commandName != "POS" && commandName != "TRACK" && commandName != "collectDebugInfo";
}

bool SocketTransfer::isCachedSyncCommand(const std::string& command) {
    return command == "DFRAME" || isCachedSpeakingCommand(command);
}

bool SocketTransfer::isCachedSpeakingCommand(const std::string& command) {
    return command == "IS_SPEAKING" ||
           command.rfind("IS_SPEAKING\t", 0) == 0 ||
           command.rfind("IS_SPEAKING_BULK\t", 0) == 0;
}

std::string SocketTransfer::defaultCachedSyncResponse(const std::string& command) {
    if (command == "DFRAME") {
        return "OK";
    }
    if (command.rfind("IS_SPEAKING_BULK\t", 0) != 0) {
        return "00";
    }

    const auto playerCount = std::count(command.begin(), command.end(), '\t');
    std::string response;
    response.reserve(playerCount * 3);
    for (std::size_t index = 0; index < playerCount; ++index) {
        if (index > 0) {
            response += '\t';
        }
        response += "00";
    }
    return response;
}

std::string SocketTransfer::cachedSyncResponse(const std::string& command) {
    std::lock_guard<std::mutex> lock(cachedSyncMutex_);
    if (const auto found = cachedSyncResponses_.find(command); found != cachedSyncResponses_.end()) {
        return found->second;
    }
    return defaultCachedSyncResponse(command);
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
