#include <stdlib.h>
#include "random.h"
#include "soft_timer_delay.h"
#include "poisson_clock.h"
#include "clock_convergence.h"
#include "config.h"

#define RAND_DOUBLE (random_rand16() / ((float)(1<<16)))


static struct {
    struct poisson_timer timer;
    double scale;
    double offset;
    double time_0;
} cfg;


double clock_convergence_system_time()
{
    double time = ((double)soft_timer_time()) / SOFT_TIMER_FREQUENCY;
    time = cfg.offset + time * cfg.scale;
    time -= cfg.time_0;
    return time;
}


void clock_convergence_init(double time_scale, double time_scale_random,
        double time_offset_random)
{
    cfg.time_0 = 0.0;
    cfg.offset = time_offset_random * RAND_DOUBLE;
    cfg.scale = time_scale + time_scale_random * RAND_DOUBLE;
}


static void clock_convergence_handler(handler_arg_t arg)
{
    (void)arg;
    double time = clock_convergence_system_time();
    MSG("Clock;%f\n", time);
}


int clock_convergence_start(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    double lambda = atof(argv[1]);

    // We want divergence, not base offset
    cfg.time_0 = clock_convergence_system_time();

    INFO("%s %f\n", argv[0], lambda);
    poisson_timer_start(&cfg.timer, lambda,
            clock_convergence_handler, (handler_arg_t)&cfg);

    return 0;
}


int clock_convergence_stop(int argc, char **argv)
{
    if (argc != 1)
        return 1;

    INFO("%s\n", argv[0]);
    poisson_timer_stop(&cfg.timer);

    return 0;
}
