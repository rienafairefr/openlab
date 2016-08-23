#include "soft_timer_delay.h"
#include "iotlab_time.h"

// 1000 real soft timer frequency
#define SOFT_TIMER_KFREQUENCY_FIX 32772000

static inline uint32_t get_microseconds(uint64_t timer_tick, uint32_t kfrequency);
static inline uint32_t get_seconds(uint64_t timer_tick, uint32_t kfrequency);
static inline void ticks_conversion(struct soft_timer_timeval* time,
        uint64_t timer_tick, uint32_t kfrequency);
static uint64_t get_extended_time(uint32_t timer_tick, uint64_t timer_tick_64);


static uint64_t time0 = 0;
static struct soft_timer_timeval unix_time_ref = {0, 0};

void iotlab_time_set_time(uint32_t t0, struct soft_timer_timeval *time_ref)
{
    uint64_t now64 = soft_timer_time_64();

    time0 = get_extended_time(t0, now64);

    unix_time_ref = *time_ref;
}


static void iotlab_time_convert(struct soft_timer_timeval *time, uint64_t timer_tick_64)
{
    /*
     * Frequency scaling should only be used to convert the ticks
     *       between 'time0' and 'timer_tick_64'.
     */
    ticks_conversion(time, timer_tick_64 - time0, SOFT_TIMER_KFREQUENCY_FIX);

    /* Add unix time */
    time->tv_sec  += unix_time_ref.tv_sec;
    time->tv_usec += unix_time_ref.tv_usec;

    /* Correct usecs > 100000 */
    if (time->tv_usec > 1000000) {
        time->tv_sec  += 1;
        time->tv_usec -= 1000000;
    }
}


void iotlab_time_extend_relative(struct soft_timer_timeval *time,
        uint32_t timer_tick)
{
    uint64_t now64 = soft_timer_time_64();
    uint64_t timer_tick_64 = get_extended_time(timer_tick, now64);

    iotlab_time_convert(time, timer_tick_64);
}



/*
 * Extend to 64bit a past 32 bits 'ticks' timer using current 64 ticks timer.
 */
static uint64_t get_extended_time(uint32_t timer_tick, uint64_t timer_tick_64)
{
    // Get the big part from the 64 bit timer, and the small from the 32 bit timer.
    uint64_t absolute_time = (timer_tick_64 & (~0xFFFFFFFFull)) | timer_tick;

    // remove the 'overflow' if necessary
    if ((timer_tick_64 & 0x80000000) < (timer_tick & 0x80000000))
        absolute_time -= 0x100000000;

    return absolute_time;
}

static inline void ticks_conversion(struct soft_timer_timeval* time, uint64_t timer_tick, uint32_t kfrequency)
{
    time->tv_sec = get_seconds(timer_tick, kfrequency);
    time->tv_usec = get_microseconds(timer_tick, kfrequency);
}

static inline uint32_t get_seconds(uint64_t timer_tick, uint32_t kfrequency)
{
    timer_tick *= 1000;  // Use 1000 * ticks as using 1000 * frequency

    return timer_tick / kfrequency;
}

static inline uint32_t get_microseconds(uint64_t timer_tick, uint32_t kfrequency)
{
    timer_tick *= 1000;  // Use 1000 * ticks as using 1000 * frequency

    uint64_t aux64;
    aux64 = (timer_tick % kfrequency);
    aux64 *= 1000000;  // convert to useconds before dividing to keep as integer
    aux64 /= kfrequency;
    return (uint32_t)aux64;
}
