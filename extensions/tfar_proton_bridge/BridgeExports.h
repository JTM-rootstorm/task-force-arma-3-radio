#pragma once

#include <cstdint>

#define TFAR_BRIDGE_API_VERSION 1u

extern "C" __declspec(dllexport)
std::uint32_t __stdcall TFARBridge_GetApiVersion();

extern "C" __declspec(dllexport)
void __stdcall TFARBridge_RVExtension(char* output, int outputSize, const char* input);

extern "C" __declspec(dllexport)
void __stdcall TFARBridge_Shutdown();
