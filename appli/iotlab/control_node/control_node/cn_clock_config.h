#ifndef CN_CLOCK_H
#define CN_CLOCK_H

/** Start the clock library */
void cn_clock_start();
void cn_clock_handler(struct iotlab_time_config *config);

void flush_current_clock();

#endif//CN_CLOCK_H
