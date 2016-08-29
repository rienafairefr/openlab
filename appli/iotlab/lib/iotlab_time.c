#include <string.h>
#include "soft_timer_delay.h"
#include "iotlab_time.h"

// 1000 real soft timer frequency
//     72000000. / (72000000 / 32768) == 32771.96176604461
#define SOFT_TIMER_KFREQUENCY_FIX 32771798

static inline uint32_t get_microseconds(uint64_t timer_tick, uint32_t kfrequency);
static inline uint32_t get_seconds(uint64_t timer_tick, uint32_t kfrequency);
static inline void ticks_conversion(struct soft_timer_timeval* time,
        uint64_t timer_tick, uint32_t kfrequency);
static uint64_t get_extended_time(uint32_t timer_tick, uint64_t timer_tick_64);

struct iotlab_time_config {
    uint64_t time0;
    struct soft_timer_timeval unix_time_ref;
    uint32_t kfrequency;  // 1000 * frequency to get more precisions with int
    uint64_t last_set_time;
};

/* Number of previous config saved */
#define NUM_SAVED_CONFIG (1)
/* Sorted from latest to oldest */
static struct iotlab_time_config time_config[1 + NUM_SAVED_CONFIG] = {
    [0 ... NUM_SAVED_CONFIG] =
    {0, {0, 0}, SOFT_TIMER_KFREQUENCY_FIX, 0}};


void iotlab_time_set_time(uint32_t t0, struct soft_timer_timeval *time_ref)
{
    memmove(time_config + 1, time_config, NUM_SAVED_CONFIG * sizeof(struct iotlab_time_config));

    uint64_t now64 = soft_timer_time_64();
    uint64_t time0 = get_extended_time(t0, now64);

    time_config[0].time0 = time0;
    time_config[0].unix_time_ref = *time_ref;
    time_config[0].kfrequency = SOFT_TIMER_KFREQUENCY_FIX;
    time_config[0].last_set_time = now64;
}

static struct iotlab_time_config *select_config(uint64_t timer_tick_64)
{
    uint64_t last_set_time = time_config[0].last_set_time;

    if (timer_tick_64 < last_set_time)
        return &time_config[1];
    /* If the timer_tick_64 is equal to the last set time, we use previous config */
    else if (timer_tick_64 == last_set_time)
        return &time_config[1];
    else
        return &time_config[0];
}

static void _iotlab_time_convert(struct iotlab_time_config *config, struct soft_timer_timeval *time, uint64_t timer_tick_64)
{
    /*
     * Frequency scaling should only be used to convert the ticks
     *       between 'time0' and 'timer_tick_64'.
     */
    ticks_conversion(time, timer_tick_64 - config->time0, config->kfrequency);

    /* Add unix time */
    time->tv_sec  += config->unix_time_ref.tv_sec;
    time->tv_usec += config->unix_time_ref.tv_usec;

    /* Correct usecs > 100000 */
    if (time->tv_usec > 1000000) {
        time->tv_sec  += 1;
        time->tv_usec -= 1000000;
    }
}

static void iotlab_time_convert(struct soft_timer_timeval *time, uint64_t timer_tick_64)
{
    struct iotlab_time_config *config = select_config(timer_tick_64);
    _iotlab_time_convert(config, time, timer_tick_64);
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
