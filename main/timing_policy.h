#pragma once
#include <cstdint>

namespace timing_policy {
constexpr bool IdleExpired(int64_t started_us, int64_t now_us) {
    return started_us > 0 && now_us >= started_us && now_us - started_us >= 20000000;
}
constexpr int CountdownSeconds(int64_t deadline_us, int64_t now_us) {
    return now_us >= deadline_us ? 0 : static_cast<int>((deadline_us - now_us + 999999) / 1000000);
}
}  // namespace timing_policy
