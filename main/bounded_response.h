#pragma once

#include <cstddef>

// A request owns its buffer. Never reuse a prior or truncated response.
template <std::size_t Capacity>
class BoundedResponse {
public:
    constexpr bool Append(const char* data, std::size_t length) {
        if (failed_ || (data == nullptr && length != 0) || length >= Capacity - size_) {
            failed_ = true;
            return false;
        }
        for (std::size_t i = 0; i < length; ++i) data_[size_++] = data[i];
        data_[size_] = '\0';
        return true;
    }
    constexpr const char* Data() const { return data_; }
    constexpr std::size_t Size() const { return size_; }
    constexpr bool Failed() const { return failed_; }
private:
    static_assert(Capacity > 0);
    char data_[Capacity]{};
    std::size_t size_ = 0;
    bool failed_ = false;
};
