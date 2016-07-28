#include "soft_timer_delay.h"
#include "iotlab_time.h"

#define SOFT_TIMER_FREQUENCY_FIX 32772

static inline uint32_t get_microseconds(uint64_t timer_tick, uint32_t frequency);
static inline uint32_t get_seconds(uint64_t timer_tick, uint32_t frequency);
static inline void ticks_conversion(struct soft_timer_timeval* time,
        uint64_t timer_tick, uint32_t frequency);
static void get_absolute_time(struct soft_timer_timeval *absolute_time,
        uint32_t timer_tick, uint32_t timer_secs);
static uint64_t get_extended_time(uint32_t timer_tick, uint64_t timer_tick_64) __attribute__ ((unused));  // Before using


struct soft_timer_timeval time0 = {0, 0};
struct soft_timer_timeval unix_time_ref = {0, 0};

void iotlab_time_set_time(struct soft_timer_timeval *t0,
        struct soft_timer_timeval *time_ref)
{
    time0 = *t0;
    unix_time_ref = *time_ref;
}


static void iotlab_time_convert(struct soft_timer_timeval *time)
{
    /* Set time relative to time0 */
    time->tv_sec -= time0.tv_sec;
    if (time->tv_usec < time0.tv_usec) {
        time->tv_usec += 1000000;
        time->tv_sec  -= 1;
    }
    time->tv_usec -= time0.tv_usec;

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
    /* extend timer_tick to timeval */
    *time = soft_timer_time_extended();
    get_absolute_time(time, timer_tick, time->tv_sec);
    iotlab_time_convert(time);
}



/*
 * Get the absolute time using a 'ticks' timer, and a 'seconds' timer
 *
 * The 'seconds' timer should be have a value taken after the 'ticks' timer.
 * The function supports timer_secs to be taken 0xF seconds after measuring timer_tick.
 *
 */
static void get_absolute_time(struct soft_timer_timeval *absolute_time,
        uint32_t timer_tick, uint32_t timer_secs)
{
    uint32_t seconds_s     = timer_secs;
    uint32_t seconds_ticks = get_seconds(timer_tick, SOFT_TIMER_FREQUENCY_FIX);

    uint32_t final_secs;


    /*
     *
     * Get the small variations from the timer_ticks calculation of seconds
     * And the big part of the value from the timer_seconds
     *
     * In the case where the fact that timer_seconds is taken later,
     * makes its 4 LSB bytes "overflow", 0x10 should be removed
     * It's detected comparing the 5th LSB of both timers calculation of seconds.
     *
     * Example:
     * seconds_ticks = 0x0F0A
     * seconds_s = 0x0F0C (taken 2 seconds later)
     * final_secs = 0x0F00 | 0x000A == 0x0F00A
     * final_secs -= 0  // everything fine
     * final_secs == 0x0F0A == seconds_ticks (which is right for small values)
     *
     * seconds_ticks = 0x0FFE
     * seconds_s = 0x1000 (taken 2 seconds later) // here, 0x10 seconds should be removed
     * final_secs = 0x1000 | 0x000E == 0x100E
     * final_secs -= 0x10
     * final_secs = 0x0FFE == seconds_ticks (whics is what is expected for small values)
     *
     */

    // Works because 2**32 is a multiple of 32768
    // Get the big part from the seconds, and the small from the ticks.
    final_secs = (seconds_s & (~ 0xF)) | (seconds_ticks & (0xF));
    // remove the 'overflow' if necessary
    final_secs -= (seconds_s & (0x10)) ^ (seconds_ticks & (0x10));

    absolute_time->tv_sec = final_secs;
    absolute_time->tv_usec = get_microseconds(timer_tick, SOFT_TIMER_FREQUENCY_FIX);
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

static inline void ticks_conversion(struct soft_timer_timeval* time, uint64_t timer_tick, uint32_t frequency)
{
    time->tv_sec = get_seconds(timer_tick, frequency);
    time->tv_usec = get_microseconds(timer_tick, frequency);
}

static inline uint32_t get_seconds(uint64_t timer_tick, uint32_t frequency)
{
    return timer_tick / frequency;
}

static inline uint32_t get_microseconds(uint64_t timer_tick, uint32_t frequency)
{
    uint64_t aux64;
    aux64 = (timer_tick % frequency);
    aux64 *= 1000000;  // convert to useconds before dividing to keep as integer
    aux64 /= frequency;
    return (uint32_t)aux64;
}
