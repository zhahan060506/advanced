#include "gesture_policy.h"
using gesture_policy::Gesture;
constexpr bool FilterChecks() {
    gesture_policy::Filter filter;
    if (filter.Poll(Gesture::kLeft, 0) != Gesture::kNone) return false;
    if (filter.Poll(Gesture::kNone, 399999) != Gesture::kNone) return false;
    if (filter.Poll(Gesture::kNone, 400000) != Gesture::kLeft) return false;
    if (filter.Poll(Gesture::kRight, 600000) != Gesture::kNone) return false;
    if (filter.Poll(Gesture::kRight, 1300000) != Gesture::kNone) return false;
    if (filter.Poll(Gesture::kNone, 2100000) != Gesture::kNone) return false;
    if (filter.Poll(Gesture::kRight, 2200000) != Gesture::kNone) return false;
    if (filter.Poll(Gesture::kNone, 2600000) != Gesture::kRight) return false;
    return true;
}
constexpr bool DepthAndAmbiguityChecks() {
    gesture_policy::Filter filter;
    if (filter.Poll(Gesture::kUnknown, 0) != Gesture::kNone) return false;
    if (filter.Poll(Gesture::kClockwise, 200000) != Gesture::kNone) return false;
    filter.Poll(Gesture::kLeft, 400000);
    filter.Poll(Gesture::kBackward, 600000);
    if (filter.Poll(Gesture::kNone, 800000) != Gesture::kBackward) return false;
    filter.Poll(Gesture::kWave, 1600000);
    return filter.Poll(Gesture::kNone, 2000000) == Gesture::kWave;
}
static_assert(FilterChecks());
static_assert(DepthAndAmbiguityChecks());
static_assert(gesture_policy::VolumeAfter(95, Gesture::kUp) == 100);
static_assert(gesture_policy::VolumeAfter(5, Gesture::kDown) == 0);
static_assert(gesture_policy::VolumeAfter(50, Gesture::kUp) == 60);
static_assert(gesture_policy::VolumeAfter(50, Gesture::kDown) == 40);
