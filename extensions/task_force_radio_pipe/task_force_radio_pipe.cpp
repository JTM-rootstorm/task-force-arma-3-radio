// task_force_radio_pipe.cpp: Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "NamedPipeTransfer.h"
#ifdef TFAR_USE_SOCKET_BRIDGE
#include "SocketTransfer.h"
extern SocketTransfer transfer;
#else
#include "SharedMemoryTransfer.h"
extern SharedMemoryTransfer transfer;
#endif
//extern NamedPipeTransfer transfer;

void __stdcall RVExtension(char *output, int outputSize, const char *input) {
	if (input[0] == '\0')
		return;
	transfer.transactMessage(output, outputSize, input);
	output[outputSize - 1] = 0;
}
