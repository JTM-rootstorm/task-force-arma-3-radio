#ifndef _WIN32

#include <cstddef>

#include "../../include/public_definitions.h"
#include "../../include/ts3_functions.h"
#include "../transport/LinuxBridgeServer.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>

namespace {

constexpr int kPluginApiVersion = 21;
std::unique_ptr<tfar::LinuxBridgeServer> bridgeServer;

char* copyPluginString(const char* text) {
    const auto length = std::strlen(text) + 1;
    auto* result = static_cast<char*>(std::malloc(length));
    if (result != nullptr) {
        std::memcpy(result, text, length);
    }
    return result;
}

} // namespace

extern "C" {

__attribute__((visibility("default"))) const char* ts3plugin_name() {
    return "TFAR Linux Bridge";
}

__attribute__((visibility("default"))) const char* ts3plugin_version() {
    return "0.0.0-linux-alpha";
}

__attribute__((visibility("default"))) int ts3plugin_apiVersion() {
    return kPluginApiVersion;
}

__attribute__((visibility("default"))) const char* ts3plugin_author() {
    return "TFAR contributors";
}

__attribute__((visibility("default"))) const char* ts3plugin_description() {
    return "Experimental Linux/Proton TFAR bridge skeleton. Not a playable port yet.";
}

__attribute__((visibility("default"))) void ts3plugin_setFunctionPointers(const struct TS3Functions) {}

__attribute__((visibility("default"))) int ts3plugin_init() {
    bridgeServer = std::make_unique<tfar::LinuxBridgeServer>();
    return bridgeServer->initialize() ? 0 : 1;
}

__attribute__((visibility("default"))) void ts3plugin_shutdown() {
    if (bridgeServer) {
        bridgeServer->shutdown();
        bridgeServer.reset();
    }
}

__attribute__((visibility("default"))) int ts3plugin_offersConfigure() {
    return 0;
}

__attribute__((visibility("default"))) void ts3plugin_registerPluginID(const char*) {}

__attribute__((visibility("default"))) const char* ts3plugin_commandKeyword() {
    return "tfar";
}

__attribute__((visibility("default"))) int ts3plugin_processCommand(unsigned long long, const char*) {
    return 0;
}

__attribute__((visibility("default"))) void ts3plugin_currentServerConnectionChanged(unsigned long long) {}

__attribute__((visibility("default"))) const char* ts3plugin_infoTitle() {
    return "TFAR";
}

__attribute__((visibility("default"))) void ts3plugin_infoData(unsigned long long, unsigned long long, enum PluginItemType, char** data) {
    *data = copyPluginString("[B]TFAR Linux Bridge[/B]\nConnected to Game: [B]No[/B]");
}

__attribute__((visibility("default"))) void ts3plugin_freeMemory(void* data) {
    std::free(data);
}

__attribute__((visibility("default"))) int ts3plugin_requestAutoload() {
    return 0;
}

}

#endif
