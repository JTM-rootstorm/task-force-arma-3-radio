#include "stdafx.h"
#include "BridgeExports.h"
#include "SocketTransfer.h"

namespace {

SocketTransfer& bridgeTransfer() {
	static SocketTransfer transfer;
	return transfer;
}

} // namespace

extern "C" __declspec(dllexport)
std::uint32_t __stdcall TFARBridge_GetApiVersion() {
	return TFAR_BRIDGE_API_VERSION;
}

extern "C" __declspec(dllexport)
void __stdcall TFARBridge_RVExtension(char* output, int outputSize, const char* input) {
	if (output == nullptr || outputSize <= 0) {
		return;
	}

	output[0] = '\0';

	if (input == nullptr || input[0] == '\0') {
		return;
	}

	bridgeTransfer().transactMessage(output, outputSize, input);
	output[outputSize - 1] = '\0';
}

extern "C" __declspec(dllexport)
void __stdcall TFARBridge_Shutdown() {
	// Reserved for future explicit cleanup. The socket transport is process-lifetime.
}
