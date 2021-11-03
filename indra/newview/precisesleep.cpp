#include "precisesleep.h"

#include <chrono>
#include <thread>
#include <math.h>
#include <lltimer.h>

// Precise sleep, sleeps for a fraction of the actual time, then spin locks
// Spin lock idea from: https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/
// Fractional waiting idea from: https://discourse.differentk.fyi/


#define MAX_TOTAL_TIME 3e3
#define TIME_DIVISOR 3.0
#define SPIN_LOCK_THRESHOLD 5000

F64 gPercentInSpin = 0.0;
F64 gPreciseSleepFraction = 0.875;

using namespace std;
using namespace std::chrono;

#define now high_resolution_clock::now
#define time_since(start) (now() - start).count()

void precise_sleep(U64 microseconds)
{
    auto start = now();

    // Statistics for tracking performance
    static F64 total_in_sleep = 0;
    static F64 total_in_spin = 0;
    
    // Reset so we show only recent stats
    if (total_in_sleep + total_in_spin > MAX_TOTAL_TIME)
    {
        total_in_sleep /= TIME_DIVISOR;
        total_in_spin /= TIME_DIVISOR;
    }

    // Sleep for a specified fraction of the desired time
    S64 nanoseconds = (microseconds * (S64)1000);
    S64 sleep_time = (S64)(nanoseconds * gPreciseSleepFraction);
    micro_sleep(sleep_time / 1000);
    S64 time_slept = time_since(start);
    S64 remaining_time = nanoseconds - time_slept;
    total_in_sleep += time_slept / 1e6;

    // spin lock, if time remaining is above threshold
    if (remaining_time > SPIN_LOCK_THRESHOLD)
    {
        auto start = now();
        while (time_since(start) < remaining_time - SPIN_LOCK_THRESHOLD / 2)
        {
#if LL_WINDOWS
            _YIELD_PROCESSOR();
#endif
        }
        total_in_spin += time_since(start) / 1e6;
    }

    gPercentInSpin = total_in_spin * 100.0 / (total_in_sleep + total_in_spin);
}