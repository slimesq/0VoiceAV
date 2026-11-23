#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include <chrono>
#include <cstdint>

class TimesUtil
{
public:
    static int64_t GetTimeMillisecond() noexcept
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
};

#endif // TIMEUTIL_H
