#include "stdafx.h"
#include "RuntimeTransportSelector.h"

#include "BridgeLoader.h"
#include "ProtonDetection.h"
#include "SharedMemoryTransfer.h"

#include <algorithm>
#include <cstring>
#include <mutex>

namespace tfar_pipe {
namespace {

class RuntimeTransportSelector {
public:
	void transactMessage(char* output, int outputSize, const char* input) {
		ensureInitialized();

		if (selectedBridge_) {
			bridge_.callRvExtension(output, outputSize, input);
			terminateOutput(output, outputSize);
			return;
		}

		if (forcedBridgeFailed_) {
			writeOutput(output, outputSize, "TFAR Proton bridge unavailable");
			return;
		}

		sharedMemory_.transactMessage(output, outputSize, input);
		terminateOutput(output, outputSize);
	}

	void shutdown() {
		bridge_.shutdown();
	}

private:
	void ensureInitialized() {
		std::call_once(initOnce_, [this]() {
			const bool bridgeExists = BridgeLoader::bridgeExists();
			const ProtonBridgeDecision decision = decideProtonBridgeUse(bridgeExists);
			if (!decision.shouldTryBridge) {
				return;
			}

			if (bridge_.load()) {
				selectedBridge_ = true;
				return;
			}

			if (decision.forced) {
				forcedBridgeFailed_ = true;
			}
		});
	}

	static void terminateOutput(char* output, int outputSize) {
		if (output != nullptr && outputSize > 0) {
			output[outputSize - 1] = '\0';
		}
	}

	static void writeOutput(char* output, int outputSize, const char* text) {
		if (output == nullptr || outputSize <= 0) {
			return;
		}
		if (text == nullptr) {
			output[0] = '\0';
			return;
		}

		const std::size_t limit = static_cast<std::size_t>(outputSize - 1);
		const std::size_t count = std::min(std::strlen(text), limit);
		std::memcpy(output, text, count);
		output[count] = '\0';
	}

	std::once_flag initOnce_;
	SharedMemoryTransfer sharedMemory_;
	BridgeLoader bridge_;
	bool selectedBridge_ = false;
	bool forcedBridgeFailed_ = false;
};

bool& selectorCreated() {
	static bool created = false;
	return created;
}

RuntimeTransportSelector& selector() {
	static RuntimeTransportSelector instance;
	selectorCreated() = true;
	return instance;
}

} // namespace

void transactRuntime(char* output, int outputSize, const char* input) {
	if (output != nullptr && outputSize > 0) {
		output[0] = '\0';
	}

	if (output == nullptr || outputSize <= 0) {
		return;
	}

	if (input == nullptr || input[0] == '\0') {
		return;
	}

	selector().transactMessage(output, outputSize, input);
	output[outputSize - 1] = '\0';
}

void shutdownRuntime() {
	if (selectorCreated()) {
		selector().shutdown();
	}
}

} // namespace tfar_pipe
