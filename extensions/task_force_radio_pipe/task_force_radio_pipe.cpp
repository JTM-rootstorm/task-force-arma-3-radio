// task_force_radio_pipe.cpp: Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "RuntimeTransportSelector.h"

extern "C" __declspec(dllexport)
void __stdcall RVExtension(char *output, int outputSize, const char *input) {
	tfar_pipe::transactRuntime(output, outputSize, input);
}
