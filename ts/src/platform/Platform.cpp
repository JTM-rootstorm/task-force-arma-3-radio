#include "Platform.hpp"

#include <cstdlib>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace tfar::platform {

void sleepFor(std::chrono::milliseconds duration) {
    std::this_thread::sleep_for(duration);
}

void debugLog(const std::string& message) {
#ifdef _WIN32
    OutputDebugStringA(message.c_str());
#else
    (void)message;
#endif
}

bool hasLoadedLegacyTfarPlugin() {
#ifdef _WIN32
    return GetModuleHandleA("task_force_radio_win32") ||
        GetModuleHandleA("task_force_radio_win64") ||
        GetModuleHandleA("TFAR_dev_win64");
#else
    return false;
#endif
}

void showDuplicatePluginWarning(const std::string& pluginPath) {
#ifdef _WIN32
    MessageBoxA(
        nullptr,
        (std::string("Multiple TFAR plugins are loaded. You probably need to disable the old \"Task Force Arma 3 Radio\" plugin and delete the dll file manually.\n"
                     "The plugins are probably in this directory: ") + pluginPath).c_str(),
        "Task Force Arrowhead Radio",
        MB_OK | MB_ICONHAND);
#else
    (void)pluginPath;
#endif
}

std::string appendFileName(const std::string& directory, const std::string& fileName) {
    if (directory.empty()) {
        return fileName;
    }
    const char separator =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif
    if (directory.back() == '/' || directory.back() == '\\') {
        return directory + fileName;
    }
    return directory + separator + fileName;
}

std::string userConfigDirectory() {
#ifdef _WIN32
    const char* appData = std::getenv("appdata");
    if (appData != nullptr) {
        return appendFileName(appData, "TS3Client");
    }
    return {};
#else
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig != nullptr && *xdgConfig != '\0') {
        return appendFileName(xdgConfig, "tfar");
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
        return appendFileName(appendFileName(home, ".config"), "tfar");
    }
    return {};
#endif
}

} // namespace tfar::platform
