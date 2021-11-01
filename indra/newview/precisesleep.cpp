#include "precisesleep.h"

#include <chrono>
#include <thread>
#include <math.h>

// Modified version of:
// https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/


#define MAX_U64 (UINT64_MAX - 1500000)
#define MAX_F64 1e150
#define INIT_ESTIMATE 5000000


void precise_sleep(U64 microseconds)
{
    using namespace std;
    using namespace std::chrono;

    static const duration _1ms = milliseconds(1);

    static S64 estimate = INIT_ESTIMATE;
    static F64 mean = INIT_ESTIMATE;
    static F64 m2 = 0;
    static U64 count = 1;

    // Need to reset if we approach any limit,
    // but we'll be back to normal after ~500 counts
    if (count > MAX_U64 || m2 > MAX_F64)
    {
        m2 = 0;
        count = 1;
    }

    S64 nanoseconds = microseconds * (S64)1000;
    while (nanoseconds > estimate) {
        auto start = high_resolution_clock::now();
        this_thread::sleep_for(_1ms);
        auto end = high_resolution_clock::now();

        S64 observed = (end - start).count();
        nanoseconds -= observed;

        ++count;
        F64 delta = observed - mean;
        mean += delta / count;
        m2 += delta * (observed - mean);
        F64 stddev = sqrt(m2 / (count - 1));
        estimate = (S64) (mean + stddev);
    }

    // spin lock
    auto start = high_resolution_clock::now();
    while ((high_resolution_clock::now() - start).count() < nanoseconds);
}

/*

double gFractionInSpin;

void precise_sleep(U64 microseconds) {
    using namespace std;
    using namespace std::chrono;

    double seconds = microseconds / 1000000.0;

    static double estimate = 5e-3;
    static double mean = 5e-3;
    static double m2 = 0;
    static int64_t count = 1;

    static double total_in_sleep = 0;
    static double total_in_spin = 0;

    auto stopclock = high_resolution_clock::now();

    while (seconds > estimate) {
        auto start = high_resolution_clock::now();
        this_thread::sleep_for(milliseconds(1));
        auto end = high_resolution_clock::now();

        double observed = (end - start).count() / 1e9;
        seconds -= observed;

        ++count;
        double delta = observed - mean;
        mean += delta / count;
        m2 += delta * (observed - mean);
        double stddev = sqrt(m2 / (count - 1));
        estimate = mean + stddev;
    }

    total_in_sleep += (high_resolution_clock::now() - stopclock).count() / 1e6;

    stopclock = high_resolution_clock::now();

    // spin lock
    auto start = high_resolution_clock::now();
    while ((high_resolution_clock::now() - start).count() / 1e9 < seconds);

    total_in_spin += (high_resolution_clock::now() - stopclock).count() / 1e6;

    gFractionInSpin = total_in_spin * 100.0 / total_in_sleep;
}

*/