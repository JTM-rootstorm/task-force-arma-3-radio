#include "stdafx.h"
#include "BridgeLoader.h"

#include <array>

namespace tfar_pipe {
namespace {

constexpr std::uint32_t kExpectedBridgeApiVersion = 1;
constexpr wchar_t kBridgeDllName[] = L"tfar_proton_bridge_x64.dll";

HMODULE currentModuleFromAddress() {
	HMODULE module = nullptr;
	GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&currentModuleFromAddress),
		&module);
	return module;
}

std::wstring currentModuleDirectory() {
	std::array<wchar_t, MAX_PATH> path{};
	HMODULE module = currentModuleFromAddress();
	if (module == nullptr) {
		return {};
	}

	const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
	if (length == 0 || length >= path.size()) {
		return {};
	}

	std::wstring fullPath(path.data(), length);
	const std::wstring::size_type slash = fullPath.find_last_of(L"\\/");
	if (slash == std::wstring::npos) {
		return {};
	}
	return fullPath.substr(0, slash);
}

bool fileExists(const std::wstring& path) {
	if (path.empty()) {
		return false;
	}

	const DWORD attributes = GetFileAttributesW(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

} // namespace

std::wstring BridgeLoader::bridgePath() {
	const std::wstring directory = currentModuleDirectory();
	if (directory.empty()) {
		return {};
	}
	return directory + L"\\" + kBridgeDllName;
}

bool BridgeLoader::bridgeExists() {
	return fileExists(bridgePath());
}

bool BridgeLoader::load() {
	if (module_ != nullptr && rvExtension_ != nullptr) {
		return true;
	}

	const std::wstring path = bridgePath();
	if (path.empty()) {
		error_ = "could not resolve dispatcher DLL directory";
		return false;
	}

	if (!fileExists(path)) {
		error_ = "bridge DLL does not exist beside dispatcher";
		return false;
	}

	module_ = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	if (module_ == nullptr) {
		module_ = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
	}

	if (module_ == nullptr) {
		error_ = "LoadLibraryExW failed for bridge DLL";
		return false;
	}

	getApiVersion_ = reinterpret_cast<GetApiVersionFn>(GetProcAddress(module_, "TFARBridge_GetApiVersion"));
	rvExtension_ = reinterpret_cast<RvExtensionFn>(GetProcAddress(module_, "TFARBridge_RVExtension"));
	shutdown_ = reinterpret_cast<ShutdownFn>(GetProcAddress(module_, "TFARBridge_Shutdown"));

	if (getApiVersion_ == nullptr || rvExtension_ == nullptr || shutdown_ == nullptr) {
		error_ = "bridge DLL is missing required exports";
		return false;
	}

	if (getApiVersion_() != kExpectedBridgeApiVersion) {
		error_ = "bridge DLL API version mismatch";
		return false;
	}

	error_.clear();
	return true;
}

bool BridgeLoader::isLoaded() const {
	return module_ != nullptr && rvExtension_ != nullptr;
}

const std::string& BridgeLoader::error() const {
	return error_;
}

void BridgeLoader::callRvExtension(char* output, int outputSize, const char* input) {
	if (rvExtension_ != nullptr) {
		rvExtension_(output, outputSize, input);
	}
}

void BridgeLoader::shutdown() {
	if (shutdown_ != nullptr) {
		shutdown_();
	}
}

} // namespace tfar_pipe
