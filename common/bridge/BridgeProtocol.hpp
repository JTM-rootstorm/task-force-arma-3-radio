#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace tfar {
namespace bridge {

constexpr std::uint32_t kMagic = 0x52414654u;
constexpr std::uint16_t kVersion = 1;
constexpr std::uint32_t kDefaultPort = 47333;
constexpr std::uint32_t kMaxPayloadBytes = 64u * 1024u;
constexpr std::size_t kHeaderSize = 16;

enum class Type : std::uint16_t {
    Hello = 1,
    HelloAck = 2,
    Ping = 3,
    Pong = 4,
    SyncCommand = 5,
    AsyncCommand = 6,
    Response = 7,
    Error = 8,
    Disconnect = 9,
};

#pragma pack(push, 1)
struct Header {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t type;
    std::uint32_t sequence;
    std::uint32_t length;
};
#pragma pack(pop)

static_assert(sizeof(Header) == kHeaderSize, "TFAR bridge header must stay 16 bytes");

inline bool isKnownType(std::uint16_t raw) noexcept {
    switch (static_cast<Type>(raw)) {
        case Type::Hello:
        case Type::HelloAck:
        case Type::Ping:
        case Type::Pong:
        case Type::SyncCommand:
        case Type::AsyncCommand:
        case Type::Response:
        case Type::Error:
        case Type::Disconnect:
            return true;
    }
    return false;
}

inline bool isCommand(Type type) noexcept {
    return type == Type::SyncCommand || type == Type::AsyncCommand;
}

inline bool isValidHeader(const Header& header) noexcept {
    return header.magic == kMagic &&
        header.version == kVersion &&
        isKnownType(header.type) &&
        header.length <= kMaxPayloadBytes;
}

inline Header makeHeader(Type type, std::uint32_t sequence, std::uint32_t length) noexcept {
    return Header{ kMagic, kVersion, static_cast<std::uint16_t>(type), sequence, length };
}

inline std::array<std::uint8_t, kHeaderSize> encodeHeader(const Header& header) noexcept {
    std::array<std::uint8_t, kHeaderSize> out{};
    const auto put16 = [&out](std::size_t offset, std::uint16_t value) {
        out[offset] = static_cast<std::uint8_t>(value & 0xffu);
        out[offset + 1] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
    };
    const auto put32 = [&out](std::size_t offset, std::uint32_t value) {
        out[offset] = static_cast<std::uint8_t>(value & 0xffu);
        out[offset + 1] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
        out[offset + 2] = static_cast<std::uint8_t>((value >> 16u) & 0xffu);
        out[offset + 3] = static_cast<std::uint8_t>((value >> 24u) & 0xffu);
    };

    put32(0, header.magic);
    put16(4, header.version);
    put16(6, header.type);
    put32(8, header.sequence);
    put32(12, header.length);
    return out;
}

inline Header decodeHeader(const std::uint8_t* bytes) noexcept {
    const auto get16 = [bytes](std::size_t offset) {
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(bytes[offset]) |
            static_cast<std::uint16_t>(bytes[offset + 1]) << 8u);
    };
    const auto get32 = [bytes](std::size_t offset) {
        return static_cast<std::uint32_t>(
            static_cast<std::uint32_t>(bytes[offset]) |
            static_cast<std::uint32_t>(bytes[offset + 1]) << 8u |
            static_cast<std::uint32_t>(bytes[offset + 2]) << 16u |
            static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
    };

    return Header{ get32(0), get16(4), get16(6), get32(8), get32(12) };
}

inline std::vector<std::uint8_t> encodeFrame(Type type, std::uint32_t sequence, const std::string& payload) {
    if (payload.size() > kMaxPayloadBytes) {
        return {};
    }

    const Header header = makeHeader(type, sequence, static_cast<std::uint32_t>(payload.size()));
    const auto encodedHeader = encodeHeader(header);
    std::vector<std::uint8_t> frame;
    frame.reserve(encodedHeader.size() + payload.size());
    frame.insert(frame.end(), encodedHeader.begin(), encodedHeader.end());
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

inline bool containsToken(const std::string& helloPayload, const std::string& token) {
    return token.empty() || helloPayload.find("\"token\":\"" + token + "\"") != std::string::npos;
}

} // namespace bridge
} // namespace tfar
