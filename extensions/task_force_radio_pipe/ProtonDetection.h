#pragma once

namespace tfar_pipe {

struct ProtonBridgeDecision {
	bool shouldTryBridge = false;
	bool forced = false;
	const char* reason = "not evaluated";
};

bool envFlagEnabled(const char* name);
bool envVarPresent(const char* name);
bool isRunningUnderWine();
bool hasProtonEnvironmentHint();
bool shouldForceProtonBridge();
bool shouldDisableProtonBridge();
ProtonBridgeDecision decideProtonBridgeUse(bool bridgeDllExists);

} // namespace tfar_pipe
