#include "servo_motion.h"
using namespace servo_motion;
static_assert(PulseUs(90) == 1500);
static_assert(PulseUs(-100) == PulseUs(60));
static_assert(PulseUs(1000) == PulseUs(120));
static_assert(PulseUs(60) >= 1300 && PulseUs(120) <= 1700);
static_assert(Duty(90) == 1229);
static_assert(Step(90, 120) == 91);
static_assert(Step(90, 60) == 89);
static_assert(Step(90, 90) == 90);
static_assert(Step(60, -1) == 60);
static_assert(Step(120, 200) == 120);
static_assert(SpeakingTarget(90, 599) == 95);
static_assert(SpeakingTarget(90, 600) == 85);
static_assert(SpeakingTarget(120, 0) == 120);
static_assert(SpeakingTarget(60, 600) == 60);
static_assert(TestTarget(0) == 90 && TestTarget(1000) == 80);
static_assert(TestTarget(2000) == 100 && TestTarget(3000) == 90);
constexpr bool MotionStaysBounded() {
    for (int start = 60; start <= 120; ++start) {
        for (int target = -10; target <= 190; target += 10) {
            int current = start;
            for (int tick = 0; tick < 60; ++tick) {
                const int next = Step(current, target);
                if (next < 60 || next > 120 || next - current > 1 || current - next > 1) return false;
                current = next;
            }
            if (current != Clamp(target)) return false;
        }
    }
    return true;
}
static_assert(MotionStaysBounded());
