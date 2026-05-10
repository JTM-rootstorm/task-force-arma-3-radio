#include "WinSharedMemoryTransport.hpp"

#ifdef _WIN32

namespace tfar {

WinSharedMemoryTransport::WinSharedMemoryTransport() {
    handler_.onConnected.connect([this]() { onConnected(); });
    handler_.onDisconnected.connect([this]() { onDisconnected(); });
}

bool WinSharedMemoryTransport::initialize() {
    return true;
}

void WinSharedMemoryTransport::shutdown() {
    handler_.setGameDisconnected();
}

bool WinSharedMemoryTransport::isConnected() const {
    return handler_.isConnected();
}

std::optional<GameCommand> WinSharedMemoryTransport::receiveCommand(std::chrono::milliseconds timeout) {
    std::string payload;
    if (!handler_.getData(payload, timeout)) {
        return std::nullopt;
    }

    GameCommand command;
    command.async = !payload.empty() && payload.back() == '~';
    if (command.async) {
        payload.pop_back();
    }
    command.payload = std::move(payload);
    return command;
}

bool WinSharedMemoryTransport::sendResponse(std::uint32_t, const std::string& response) {
    return handler_.sendData(response);
}

void WinSharedMemoryTransport::setConfigNeedsRefresh(bool needsRefresh) {
    handler_.setConfigNeedsRefresh(needsRefresh);
}

} // namespace tfar

#endif
