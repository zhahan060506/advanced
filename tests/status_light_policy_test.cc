// Compile-only tests: no ESP32 hardware required, assertions run in compiler.
#include "status_light_policy.h"

using status_light::Color;
using status_light::Frame;
using status_light::Mode;
using status_light::SelectMode;

// A sounding alarm/countdown must win even during speech and network errors.
static_assert(SelectMode(kDeviceStateSpeaking, true, true, true) == Mode::kRinging);
static_assert(SelectMode(kDeviceStateListening, false, true, false) == Mode::kRinging);
// Canceling the ringing overlay restores the current activity, not a stale color.
static_assert(SelectMode(kDeviceStateSpeaking, false, false, false) == Mode::kSpeaking);
static_assert(SelectMode(kDeviceStateIdle, true, false, false) == Mode::kCountdown);
static_assert(SelectMode(kDeviceStateIdle, false, false, false) == Mode::kOff);
// Countdown remains active in the background while conversation gets feedback.
static_assert(SelectMode(kDeviceStateListening, true, false, false) == Mode::kListening);
static_assert(SelectMode(kDeviceStateSpeaking, true, false, false) == Mode::kSpeaking);
// Error indication takes priority over conversation, and recovers on expiry.
static_assert(SelectMode(kDeviceStateSpeaking, false, false, true) == Mode::kError);
static_assert(SelectMode(kDeviceStateFatalError, true, false, false) == Mode::kError);
static_assert(SelectMode(kDeviceStateWifiConfiguring, false, false, false) == Mode::kWifiConfiguring);
// Blink boundaries, channel limits and off state are hardware-independent.
static_assert(Frame(Mode::kRinging, 0, 24) == Color{24, 0, 0});
static_assert(Frame(Mode::kRinging, 249, 24) == Color{24, 0, 0});
static_assert(Frame(Mode::kRinging, 250, 24) == Color{});
static_assert(Frame(Mode::kRinging, 500, 24) == Color{24, 0, 0});
static_assert(Frame(Mode::kListening, 0, 24) == Color{0, 0, 24});
static_assert(Frame(Mode::kSpeaking, 0, 24) == Color{0, 24, 0});
static_assert(Frame(Mode::kCountdown, 0, 255) == Color{32, 32, 0});
static_assert(Frame(Mode::kOff, 0, 255) == Color{});
