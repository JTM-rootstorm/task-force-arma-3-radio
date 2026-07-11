#include "../common/bridge/BridgeProtocol.hpp"

#include <array>
#include <iostream>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
        ++failures;
    }
}
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

int main() {
    using namespace tfar::bridge;

    const auto frame = encodeFrame(Type::Hello, 42, "hello");
    CHECK(frame.size() == kHeaderSize + 5);

    const auto header = decodeHeader(frame.data());
    CHECK(header.magic == kMagic);
    CHECK(header.version == kVersion);
    CHECK(header.type == static_cast<std::uint16_t>(Type::Hello));
    CHECK(header.sequence == 42);
    CHECK(header.length == 5);
    CHECK(isValidHeader(header));

    auto badMagic = header;
    badMagic.magic = 0;
    CHECK(!isValidHeader(badMagic));

    auto badVersion = header;
    badVersion.version = 99;
    CHECK(!isValidHeader(badVersion));

    auto oversized = header;
    oversized.length = kMaxPayloadBytes + 1;
    CHECK(!isValidHeader(oversized));

    auto unknownType = header;
    unknownType.type = 999;
    CHECK(!isValidHeader(unknownType));

    std::array<std::uint8_t, kHeaderSize> partial{};
    const auto encodedHeader = encodeHeader(header);
    std::copy(encodedHeader.begin(), encodedHeader.begin() + 8, partial.begin());
    const auto partialDecode = decodeHeader(partial.data());
    CHECK(partialDecode.magic == kMagic);
    CHECK(partialDecode.length == 0);

    CHECK(encodeFrame(Type::Ping, 0, {}).size() == kHeaderSize);
    CHECK(encodeFrame(Type::Response, 1, std::string(kMaxPayloadBytes, 'x')).size() == kHeaderSize + kMaxPayloadBytes);
    CHECK(encodeFrame(Type::Response, 1, std::string(kMaxPayloadBytes + 1, 'x')).empty());

    std::cout << "bridge protocol smoke tests passed\n";
    return failures == 0 ? 0 : 1;
}
