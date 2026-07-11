#pragma once

#include <cstdint>
#include <string>
#include <windows.h>

namespace tfar_pipe {

class BridgeLoader {
public:
	using GetApiVersionFn = std::uint32_t(__stdcall*)();
	using RvExtensionFn = void(__stdcall*)(char*, int, const char*);
	using ShutdownFn = void(__stdcall*)();

	static std::wstring bridgePath();
	static bool bridgeExists();

	bool load();
	bool isLoaded() const;
	const std::string& error() const;
	void callRvExtension(char* output, int outputSize, const char* input);
	void shutdown();
	void unload();

private:
	void reset();
	HMODULE module_ = nullptr;
	GetApiVersionFn getApiVersion_ = nullptr;
	RvExtensionFn rvExtension_ = nullptr;
	ShutdownFn shutdown_ = nullptr;
	std::string error_;
};

} // namespace tfar_pipe
