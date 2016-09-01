#ifndef IOTLAB_TIME_H
#define IOTLAB_TIME_H

struct iotlab_time_config {
    uint64_t time0;
    struct soft_timer_timeval unix_time_ref;
    uint32_t kfrequency;  // 1000 * frequency to get more precisions with int
    uint64_t last_set_time;
};

/*
 * Sets t0 as 64b reference in time0 and
 * save time_ref in unix_time_ref
 */
void iotlab_time_set_time(uint32_t t0, struct soft_timer_timeval *time_ref);

/*
 * Extend given timer_tick in ticks to a struct soft_timer_timeval
 * whith a value in unix timestamp
 */
void iotlab_time_extend_relative(struct soft_timer_timeval *extended_time,
        uint32_t timer_tick);
/*
 * Accessor for the current clock config in iotlab_time
 */
void iotlab_time_get_config(struct iotlab_time_config *config);

#endif // IOTLAB_TIME_H
