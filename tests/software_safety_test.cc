// Compile-time regression checks evaluate production functions without ESP32 hardware.
#include "bounded_response.h"
#include "protocols/audio_frame.h"
#include "timing_policy.h"
#include <limits>

constexpr bool BufferChunks() {
    BoundedResponse<8> buffer;
    return buffer.Append("{", 1) && buffer.Append("\"a\":1}", 6) &&
           buffer.Size() == 7 && buffer.Data()[7] == '\0' && !buffer.Failed();
}
constexpr bool BufferOverflowIsSticky() {
    BoundedResponse<8> buffer;
    return buffer.Append("1234567", 7) && !buffer.Append("8", 1) &&
           buffer.Failed() && buffer.Size() == 7 && !buffer.Append("", 0);
}
constexpr bool BufferNullAndHuge() {
    BoundedResponse<8> empty;
    BoundedResponse<8> invalid;
    BoundedResponse<8> huge;
    return empty.Append(nullptr, 0) && !invalid.Append(nullptr, 1) &&
           !huge.Append("x", std::numeric_limits<std::size_t>::max());
}
static_assert(BufferChunks());
static_assert(BufferOverflowIsSticky());
static_assert(BufferNullAndHuge());

constexpr uint8_t kV2[] = {0,2,0,0,0,0,0,0,0x12,0x34,0x56,0x78,0,0,0,3,10,11,12};
constexpr uint8_t kV3[] = {0,0,0,3,10,11,12};
constexpr uint8_t kUdp[] = {1,0,0,3,0,0,0,0,0x12,0x34,0x56,0x78,0,0,0,1,10,11,12};
constexpr bool RejectEveryTruncatedHeaderAndPayload() {
    for (std::size_t length = 0; length < sizeof(kV2); ++length) {
        if (audio_frame::Parse(kV2, length, 2).valid) return false;
        if (audio_frame::ParseUdp(kUdp, length).valid) return false;
    }
    for (std::size_t length = 0; length < sizeof(kV3); ++length) {
        if (audio_frame::Parse(kV3, length, 3).valid) return false;
    }
    return true;
}
static_assert(RejectEveryTruncatedHeaderAndPayload());
static_assert(audio_frame::Parse(kV2, sizeof(kV2), 2).timestamp == 0x12345678);
static_assert(audio_frame::Parse(kV2, sizeof(kV2), 2).offset == 16);
static_assert(audio_frame::Parse(kV3, sizeof(kV3), 3).length == 3);
static_assert(audio_frame::ParseUdp(kUdp, sizeof(kUdp)).timestamp == 0x12345678);
static_assert(!audio_frame::Parse(nullptr, 1000, 1).valid);
static_assert(!audio_frame::Parse(kV3, sizeof(kV3), 4).valid);
constexpr uint8_t kOversized[] = {0,0,255,255,10};
constexpr uint8_t kWrongType[] = {1,0,0,1,10};
static_assert(!audio_frame::Parse(kOversized, sizeof(kOversized), 3).valid);
static_assert(!audio_frame::Parse(kWrongType, sizeof(kWrongType), 3).valid);
static_assert(audio_frame::ValidConfig(24000, 60));
static_assert(!audio_frame::ValidConfig(-1, 60));
static_assert(!audio_frame::ValidConfig(44100, 60));
static_assert(!audio_frame::ValidConfig(48000, 1000000));
static_assert(audio_frame::ValidAesHex("0123456789abcdef0123456789ABCDEF"));
static_assert(!audio_frame::ValidAesHex("0123"));
static_assert(!audio_frame::ValidAesHex("z123456789abcdef0123456789ABCDEF"));
static_assert(!audio_frame::ValidAesHex("0123456789abcdef0123456789ABCDEFFF"));

static_assert(!timing_policy::IdleExpired(0, 99999999));
static_assert(!timing_policy::IdleExpired(100, 20000099));
static_assert(timing_policy::IdleExpired(100, 20000100));
// A new conversation rearms its own deadline; an earlier turn cannot expire it.
static_assert(!timing_policy::IdleExpired(19000000, 21000000));
static_assert(timing_policy::CountdownSeconds(3000000, 0) == 3);
static_assert(timing_policy::CountdownSeconds(3000000, 1000001) == 2);
static_assert(timing_policy::CountdownSeconds(3000000, 2999999) == 1);
static_assert(timing_policy::CountdownSeconds(3000000, 3000000) == 0);
static_assert(timing_policy::CountdownSeconds(3000000, 4000000) == 0);
