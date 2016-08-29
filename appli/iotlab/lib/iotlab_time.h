#ifndef IOTLAB_TIME_H
#define IOTLAB_TIME_H

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

#endif // IOTLAB_TIME_H
