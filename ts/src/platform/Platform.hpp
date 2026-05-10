#pragma once

#include <chrono>
#include <string>

namespace tfar::platform {

void sleepFor(std::chrono::milliseconds duration);
void debugLog(const std::string& message);
bool hasLoadedLegacyTfarPlugin();
void showDuplicatePluginWarning(const std::string& pluginPath);
std::string appendFileName(const std::string& directory, const std::string& fileName);
std::string userConfigDirectory();

} // namespace tfar::platform
