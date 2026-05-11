#include "stdafx.h"
#include "ProtonDetection.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace tfar_pipe {
namespace {

std::string getEnvString(const char* name) {
	if (name == nullptr) {
		return {};
	}

	const DWORD needed = GetEnvironmentVariableA(name, nullptr, 0);
	if (needed == 0) {
		return {};
	}

	std::string value(needed, '\0');
	const DWORD written = GetEnvironmentVariableA(name, &value[0], needed);
	if (written == 0 || written >= needed) {
		return {};
	}

	value.resize(written);
	return value;
}

bool equalsEnabledValue(std::string value) {
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return value == "1" || value == "true" || value == "yes" || value == "on";
}

} // namespace

bool envFlagEnabled(const char* name) {
	return equalsEnabledValue(getEnvString(name));
}

bool envVarPresent(const char* name) {
	return !getEnvString(name).empty();
}

bool isRunningUnderWine() {
	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	return ntdll != nullptr && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

bool hasProtonEnvironmentHint() {
	static constexpr std::array<const char*, 4> hints = {
		"STEAM_COMPAT_DATA_PATH",
		"STEAM_COMPAT_CLIENT_INSTALL_PATH",
		"SteamGameId",
		"SteamAppId",
	};

	for (const char* hint : hints) {
		if (envVarPresent(hint)) {
			return true;
		}
	}
	return false;
}

bool shouldForceProtonBridge() {
	return envFlagEnabled("TFAR_FORCE_PROTON_BRIDGE");
}

bool shouldDisableProtonBridge() {
	return envFlagEnabled("TFAR_DISABLE_PROTON_BRIDGE");
}

ProtonBridgeDecision decideProtonBridgeUse(bool bridgeDllExists) {
	if (shouldDisableProtonBridge()) {
		return { false, false, "disabled by TFAR_DISABLE_PROTON_BRIDGE" };
	}

	if (shouldForceProtonBridge()) {
		return { true, true, "forced by TFAR_FORCE_PROTON_BRIDGE" };
	}

	if (!isRunningUnderWine()) {
		return { false, false, "not running under Wine" };
	}

	if (hasProtonEnvironmentHint()) {
		return { true, false, "Wine plus Proton environment hint" };
	}

	if (bridgeDllExists) {
		return { true, false, "Wine plus adjacent bridge DLL" };
	}

	return { false, false, "Wine detected but no Proton hint or bridge DLL" };
}

} // namespace tfar_pipe
