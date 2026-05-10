#include "../common/bridge/BridgeProtocol.hpp"

#include <array>
#include <cassert>
#include <iostream>
#include <string>

int main() {
    using namespace tfar::bridge;

    const auto frame = encodeFrame(Type::Hello, 42, "hello");
    assert(frame.size() == kHeaderSize + 5);

    const auto header = decodeHeader(frame.data());
    assert(header.magic == kMagic);
    assert(header.version == kVersion);
    assert(header.type == static_cast<std::uint16_t>(Type::Hello));
    assert(header.sequence == 42);
    assert(header.length == 5);
    assert(isValidHeader(header));

    auto badMagic = header;
    badMagic.magic = 0;
    assert(!isValidHeader(badMagic));

    auto badVersion = header;
    badVersion.version = 99;
    assert(!isValidHeader(badVersion));

    auto oversized = header;
    oversized.length = kMaxPayloadBytes + 1;
    assert(!isValidHeader(oversized));

    std::array<std::uint8_t, kHeaderSize> partial{};
    const auto encodedHeader = encodeHeader(header);
    std::copy(encodedHeader.begin(), encodedHeader.begin() + 8, partial.begin());
    const auto partialDecode = decodeHeader(partial.data());
    assert(partialDecode.magic == kMagic);
    assert(partialDecode.length == 0);

    assert(containsToken("{\"token\":\"abc\"}", "abc"));
    assert(!containsToken("{\"token\":\"abc\"}", "def"));

    std::cout << "bridge protocol smoke tests passed\n";
    return 0;
}
