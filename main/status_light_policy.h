#ifndef STATUS_LIGHT_POLICY_H_
#define STATUS_LIGHT_POLICY_H_

#include <cstdint>
#include "device_state.h"

// Hardware-independent policy: a ring is one status display with explicit
// priorities, not several asynchronous effects overwriting each other.
namespace status_light {
enum class Mode {
    kOff, kBoot, kWifiConfiguring, kConnecting, kListening, kSpeaking,
    kCountdown, kRinging, kError, kUpgrading, kActivating,
    kTestRed, kTestGreen, kTestBlue,
};

struct Color {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    constexpr bool operator==(Color other) const {
        return red == other.red && green == other.green && blue == other.blue;
    }
};

constexpr Mode SelectMode(DeviceState state, bool counting, bool ringing, bool error) {
    if (ringing) return Mode::kRinging;
    if (error || state == kDeviceStateFatalError) return Mode::kError;
    if (state == kDeviceStateListening || state == kDeviceStateAudioTesting) return Mode::kListening;
    if (state == kDeviceStateSpeaking) return Mode::kSpeaking;
    if (counting) return Mode::kCountdown;
    switch (state) {
        case kDeviceStateUnknown:
        case kDeviceStateStarting: return Mode::kBoot;
        case kDeviceStateWifiConfiguring: return Mode::kWifiConfiguring;
        case kDeviceStateConnecting: return Mode::kConnecting;
        case kDeviceStateUpgrading: return Mode::kUpgrading;
        case kDeviceStateActivating: return Mode::kActivating;
        default: return Mode::kOff;
    }
}

constexpr Color Frame(Mode mode, uint32_t elapsed_ms, uint8_t brightness) {
    // Bound peak channel output even if a future caller bypasses configuration.
    const uint8_t level = brightness > 32 ? 32 : brightness;
    switch (mode) {
        case Mode::kRinging:
            return (elapsed_ms / 250) % 2 == 0 ? Color{level, 0, 0} : Color{};
        case Mode::kError:
            return (elapsed_ms / 150) % 2 == 0 ? Color{level, 0, 0} : Color{};
        case Mode::kBoot:
        case Mode::kWifiConfiguring:
            return (elapsed_ms / 500) % 2 == 0 ? Color{0, 0, level} : Color{};
        case Mode::kUpgrading:
            return (elapsed_ms / 125) % 2 == 0 ? Color{level, 0, level} : Color{};
        case Mode::kActivating:
            return (elapsed_ms / 500) % 2 == 0 ? Color{0, level, level} : Color{};
        case Mode::kConnecting: return {0, level, level};
        case Mode::kListening:
        case Mode::kTestBlue: return {0, 0, level};
        case Mode::kSpeaking:
        case Mode::kTestGreen: return {0, level, 0};
        case Mode::kCountdown: return {level, level, 0};
        case Mode::kTestRed: return {level, 0, 0};
        default: return {};
    }
}
}  // namespace status_light

#endif  // STATUS_LIGHT_POLICY_H_
