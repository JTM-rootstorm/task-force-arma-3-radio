#pragma once

#include "IGameTransport.hpp"

#ifdef _WIN32
#include "../SharedMemoryHandler.hpp"

namespace tfar {

class WinSharedMemoryTransport final : public IGameTransport {
public:
    WinSharedMemoryTransport();
    ~WinSharedMemoryTransport() override = default;

    bool initialize() override;
    void shutdown() override;
    bool isConnected() const override;
    std::optional<GameCommand> receiveCommand(std::chrono::milliseconds timeout) override;
    bool sendResponse(std::uint32_t sequence, const std::string& response, std::uint64_t sessionGeneration = 0) override;
    void setConfigNeedsRefresh(bool needsRefresh) override;

private:
    mutable SharedMemoryHandler handler_;
};

} // namespace tfar
#endif
