#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "../SignalSlot.hpp"

namespace tfar {

struct GameCommand {
    std::string payload;
    bool async = false;
    std::uint32_t sequence = 0;
    std::uint64_t sessionGeneration = 0;
};

class IGameTransport {
public:
    virtual ~IGameTransport() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool isConnected() const = 0;
    virtual std::optional<GameCommand> receiveCommand(std::chrono::milliseconds timeout) = 0;
    virtual bool sendResponse(std::uint32_t sequence, const std::string& response, std::uint64_t sessionGeneration = 0) = 0;
    virtual void setConfigNeedsRefresh(bool needsRefresh) = 0;

    Signal<void()> onConnected;
    Signal<void()> onDisconnected;
};

} // namespace tfar
