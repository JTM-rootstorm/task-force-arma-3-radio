#ifndef _WIN32

#include <cstddef>

#include "../../include/public_definitions.h"
#include "../../include/public_errors.h"
#include "../../include/ts3_functions.h"
#include "../transport/LinuxBridgeServer.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr int kPluginApiVersion = 26;
constexpr std::string_view kPluginVersion = "0.0.0-linux-alpha";
constexpr std::string_view kMetadataStart = "<TFAR>";
constexpr std::string_view kMetadataEnd = "</TFAR>";
std::unique_ptr<tfar::LinuxBridgeServer> bridgeServer;
TS3Functions ts3Functions{};
bool ts3FunctionsAvailable = false;

char* copyPluginString(const char* text) {
    const auto length = std::strlen(text) + 1;
    auto* result = static_cast<char*>(std::malloc(length));
    if (result != nullptr) {
        std::memcpy(result, text, length);
    }
    return result;
}

std::string bridgeStatusMetadata(bool connected) {
    std::string result;
    result.reserve(160);
    result += "Connected to Game: ";
    result += connected ? "[B]Yes[/B]" : "[B]No[/B]";
    result += "\nPlaying: [B]No[/B]";
    result += "\nPlugin version: [B]";
    result += kPluginVersion;
    result += "[/B]";
    if (connected) {
        result += "\nCommand processor: [B]Unavailable[/B]";
    }
    return result;
}

std::string replaceTfarMetadata(std::string existing, std::string_view metadata) {
    const auto start = existing.find(kMetadataStart);
    const auto end = existing.find(kMetadataEnd);
    if (start == std::string::npos || end == std::string::npos || end < start) {
        existing += kMetadataStart;
        existing += metadata;
        existing += kMetadataEnd;
        return existing;
    }

    const auto afterEnd = end + kMetadataEnd.size();
    std::string result = existing.substr(0, start);
    if (!metadata.empty()) {
        result += kMetadataStart;
        result += metadata;
        result += kMetadataEnd;
    }
    result += existing.substr(afterEnd);
    return result;
}

void setTeamSpeakMetadata(std::string_view metadata) {
    if (!ts3FunctionsAvailable ||
        ts3Functions.getCurrentServerConnectionHandlerID == nullptr ||
        ts3Functions.getClientSelfVariableAsString == nullptr ||
        ts3Functions.setClientSelfVariableAsString == nullptr ||
        ts3Functions.flushClientSelfUpdates == nullptr) {
        return;
    }

    const auto serverConnectionHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();
    if (serverConnectionHandlerID == 0) {
        return;
    }

    char* clientInfo = nullptr;
    std::string nextMetadata;
    if (ts3Functions.getClientSelfVariableAsString(serverConnectionHandlerID, CLIENT_META_DATA, &clientInfo) == ERROR_ok &&
        clientInfo != nullptr) {
        nextMetadata = replaceTfarMetadata(clientInfo, metadata);
        if (ts3Functions.freeMemory != nullptr) {
            ts3Functions.freeMemory(clientInfo);
        }
    } else {
        nextMetadata = replaceTfarMetadata({}, metadata);
    }

    ts3Functions.setClientSelfVariableAsString(serverConnectionHandlerID, CLIENT_META_DATA, nextMetadata.c_str());
    ts3Functions.flushClientSelfUpdates(serverConnectionHandlerID, nullptr);
}

void publishBridgeStatus() {
    setTeamSpeakMetadata(bridgeStatusMetadata(bridgeServer && bridgeServer->isConnected()));
}

} // namespace

extern "C" {

__attribute__((visibility("default"))) const char* ts3plugin_name() {
    return "TFAR Linux Bridge";
}

__attribute__((visibility("default"))) const char* ts3plugin_version() {
    return kPluginVersion.data();
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

__attribute__((visibility("default"))) void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
    ts3FunctionsAvailable = true;
}

__attribute__((visibility("default"))) int ts3plugin_init() {
    auto config = tfar::loadLinuxBridgeConfigFromEnvironment();
    config.rejectCommandsWithoutConsumer = true;
    bridgeServer = std::make_unique<tfar::LinuxBridgeServer>(std::move(config));
    bridgeServer->onConnected.connect([]() {
        publishBridgeStatus();
    });
    bridgeServer->onDisconnected.connect([]() {
        publishBridgeStatus();
    });
    publishBridgeStatus();
    return bridgeServer->initialize() ? 0 : 1;
}

__attribute__((visibility("default"))) void ts3plugin_shutdown() {
    setTeamSpeakMetadata({});
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

__attribute__((visibility("default"))) void ts3plugin_currentServerConnectionChanged(unsigned long long) {
    publishBridgeStatus();
}

__attribute__((visibility("default"))) const char* ts3plugin_infoTitle() {
    return "TFAR";
}

__attribute__((visibility("default"))) void ts3plugin_infoData(unsigned long long, unsigned long long, enum PluginItemType, char** data) {
    const bool connected = bridgeServer && bridgeServer->isConnected();
    *data = copyPluginString(connected
        ? "[B]TFAR Linux Bridge[/B]\nConnected to Game: [B]Yes[/B]\nCommand processor: [B]Unavailable[/B]"
        : "[B]TFAR Linux Bridge[/B]\nConnected to Game: [B]No[/B]");
}

__attribute__((visibility("default"))) void ts3plugin_freeMemory(void* data) {
    std::free(data);
}

__attribute__((visibility("default"))) int ts3plugin_requestAutoload() {
    return 0;
}

}

#endif
