#pragma once
#include <cstdint>

namespace gesture_policy {
enum class Gesture {
    kNone, kUp, kDown, kLeft, kRight, kForward, kBackward,
    kClockwise, kCounterClockwise, kWave, kUnknown,
};
constexpr bool Supported(Gesture g) {
    return (g >= Gesture::kUp && g <= Gesture::kBackward) || g == Gesture::kWave;
}
// Collect related flags for 400 ms; depth gestures take precedence over swipes.
// Require an 800 ms quiet period after an action, including repeated flags.
class Filter {
public:
    constexpr Gesture Poll(Gesture g, int64_t now) {
        if (now < quiet_until_) {
            if (Supported(g)) quiet_until_ = now + 800000;
            return Gesture::kNone;
        }
        if (Supported(g)) {
            if (pending_ == Gesture::kNone) {
                pending_ = g;
                due_ = now + 400000;
            } else if (g == Gesture::kForward || g == Gesture::kBackward) {
                pending_ = g;
            }
        }
        if (pending_ != Gesture::kNone && now >= due_) {
            const auto result = pending_;
            pending_ = Gesture::kNone;
            quiet_until_ = now + 800000;
            return result;
        }
        return Gesture::kNone;
    }
private:
    Gesture pending_ = Gesture::kNone;
    int64_t due_ = 0;
    int64_t quiet_until_ = 0;
};
constexpr int VolumeAfter(int volume, Gesture g) {
    const int next = volume + (g == Gesture::kUp ? 10 : g == Gesture::kDown ? -10 : 0);
    return next < 0 ? 0 : next > 100 ? 100 : next;
}
}
