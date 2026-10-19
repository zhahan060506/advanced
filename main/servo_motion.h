#pragma once

#include <cstdint>

// Logical angles only; calibrate the actual shaft angle on the specific servo.
namespace servo_motion {
constexpr int kMinAngle = 60;
constexpr int kMaxAngle = 120;
constexpr int kCenter = 90;
constexpr int kPeriodUs = 20000;
constexpr int kResolutionBits = 14;

constexpr int Clamp(int angle) {
    return angle < kMinAngle ? kMinAngle : (angle > kMaxAngle ? kMaxAngle : angle);
}
constexpr int PulseUs(int angle) {
    return 1000 + (Clamp(angle) * 1000 + 90) / 180;
}
constexpr uint32_t Duty(int angle) {
    return (static_cast<uint32_t>(PulseUs(angle)) * (1U << kResolutionBits)
            + kPeriodUs / 2) / kPeriodUs;
}
constexpr int Step(int current, int target) {
    target = Clamp(target);
    return current < target ? current + 1 : (current > target ? current - 1 : current);
}
constexpr int SpeakingTarget(int base, uint32_t elapsed_ms) {
    return Clamp(base + ((elapsed_ms / 600) % 2 == 0 ? 5 : -5));
}
constexpr int TestTarget(uint32_t elapsed_ms) {
    return elapsed_ms < 1000 ? 90 : (elapsed_ms < 2000 ? 80 :
           (elapsed_ms < 3000 ? 100 : 90));
}
}  // namespace servo_motion
