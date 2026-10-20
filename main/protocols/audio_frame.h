#pragma once
#include <cstddef>
#include <cstdint>

namespace audio_frame {
constexpr bool ValidConfig(int sample_rate, int duration_ms) {
    return (sample_rate == 8000 || sample_rate == 12000 || sample_rate == 16000 ||
            sample_rate == 24000 || sample_rate == 48000) && duration_ms >= 5 && duration_ms <= 120;
}
struct View {
    bool valid = false;
    std::size_t offset = 0;
    std::size_t length = 0;
    uint32_t timestamp = 0;
};
constexpr uint32_t Read32(const uint8_t* data) {
    return (uint32_t{data[0]} << 24) | (uint32_t{data[1]} << 16) |
           (uint32_t{data[2]} << 8) | data[3];
}
constexpr View Parse(const uint8_t* data, std::size_t length, int version) {
    if (!data || length == 0) return {};
    if (version == 2) {
        if (length < 16 || data[0] != 0 || data[1] != 2 || data[2] != 0 || data[3] != 0) return {};
        const auto payload = Read32(data + 12);
        if (payload == 0 || payload != length - 16) return {};
        return {true, 16, payload, Read32(data + 8)};
    }
    if (version == 3) {
        if (length < 4 || data[0] != 0) return {};
        const auto payload = (uint32_t{data[2]} << 8) | data[3];
        if (payload == 0 || payload != length - 4) return {};
        return {true, 4, payload, 0};
    }
    if (version != 1) return {};
    return {true, 0, length, 0};
}
constexpr View ParseUdp(const uint8_t* data, std::size_t length) {
    if (!data || length <= 16 || data[0] != 1) return {};
    const auto payload = (uint32_t{data[2]} << 8) | data[3];
    if (payload != length - 16) return {};
    return {true, 16, payload, Read32(data + 8)};
}
constexpr int HexDigit(char ch) {
    return ch >= '0' && ch <= '9' ? ch - '0' :
           ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 :
           ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
}
constexpr bool ValidAesHex(const char* text) {
    if (!text) return false;
    for (int i = 0; i < 32; ++i) if (HexDigit(text[i]) < 0) return false;
    return text[32] == '\0';
}
}  // namespace audio_frame
