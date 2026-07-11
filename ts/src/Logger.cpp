#include "Logger.hpp"
#include "ts3_functions.h"
#include "profilers.hpp"
#include <ctime>
#include <iomanip> // put_time
#include <utility>
#include <cstdio>

#ifdef _WIN32
#include <Windows.h>
#endif

extern struct TS3Functions ts3Functions;
FileLogger::FileLogger(const std::string& filePath) : file(filePath) {}

void FileLogger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex);
    if (file.is_open()) {
        const auto now = std::chrono::system_clock::now();
        auto inTimeT = std::chrono::system_clock::to_time_t(now);
        file << std::put_time(std::localtime(&inTimeT), "%H:%M:%S") << " " << message;
        file.flush();
    }
}

void FileLogger::log(const std::string& message, LogLevel _loglevel) {
    log(message); //#TODO add prefix according to loglevel
}

TeamspeakLogger::TeamspeakLogger(enum LogLevel defaultLoglevel) : level(defaultLoglevel) {}

void TeamspeakLogger::log(const std::string& message) {
    ts3Functions.logMessage(message.c_str(), level, "task_force_radio", 141);
}

void TeamspeakLogger::log(const std::string& message, LogLevel _loglevel) {
    ts3Functions.logMessage(message.c_str(), _loglevel, "task_force_radio", 141);
}

void CircularLogger::log(const std::string& message) {
	if (messageCount == 0) return;
	std::lock_guard<std::mutex> lock(mutex);
    messages[offset] = message;
	offset = (offset + 1) % messageCount;
}

std::vector<std::string> CircularLogger::snapshot() const {
	std::lock_guard<std::mutex> lock(mutex);
	std::vector<std::string> result;
	result.reserve(messages.size());
	for (std::size_t index = 0; index < messages.size(); ++index) {
		const auto& message = messages[(offset + index) % messages.size()];
		if (!message.empty()) result.push_back(message);
	}
	return result;
}

void CircularLogger::log(const std::string& message, LogLevel _loglevel) {
    log(message);
}

void Logger::registerLogger(LoggerTypes type, std::shared_ptr<ILogger> logger) {
    getInstance()._registerLogger(type, std::move(logger));
}

void Logger::log(LoggerTypes type, const std::string& message) {
    getInstance()._log(type, message);
}

void Logger::log(LoggerTypes type, const std::string & message, LogLevel _loglevel) {
#if DEBUG_MOD_ENABLED || isCI
    if (
    #if ENABLE_PLUGIN_LOGS
        type == LoggerTypes::pluginCommands ||
    #endif
        (_loglevel != LogLevel_DEVEL && _loglevel != LogLevel_DEBUG))
    #endif
        getInstance()._log(type, message, _loglevel);
}

std::vector<std::shared_ptr<ILogger>> Logger::getLogger(LoggerTypes type) {
	std::lock_guard<std::mutex> lock(getInstance().registryMutex);
    const auto found = getInstance().registeredLoggers.find(type);
    if (found != getInstance().registeredLoggers.end())
        return found->second;
    return {};
}

void Logger::_registerLogger(LoggerTypes type, std::shared_ptr<ILogger> logger) {
	std::lock_guard<std::mutex> lock(registryMutex);
    registeredLoggers[type].emplace_back(logger);
}

void Logger::_log(LoggerTypes type, const std::string& message) const {
	const auto loggers = getLogger(type);
	const auto formatted = !message.empty() && message.back() == '\n' ? message : message + '\n';
    for (const auto& logger : loggers) {
		logger->log(formatted);
    }
    //If not found exit silently
}

void Logger::_log(LoggerTypes type, const std::string& message, LogLevel _loglevel) const {
	const auto loggers = getLogger(type);
	const auto formatted = !message.empty() && message.back() == '\n' ? message : message + '\n';
    for (const auto& logger : loggers) {
		logger->log(formatted, _loglevel);
    }
    //If not found exit silently
}

void DebugStringLogger::log(const std::string & message) {
#ifdef _WIN32
    OutputDebugStringA(message.c_str());
#endif
    printf("%s", message.c_str());
}

void DebugStringLogger::log(const std::string & message, LogLevel _loglevel) {
    (void)_loglevel;
    log(message);
}
