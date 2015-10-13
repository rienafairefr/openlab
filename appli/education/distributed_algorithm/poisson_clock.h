#ifndef POISSON_CLOCK_H
#define POISSON_CLOCK_H

#include "soft_timer.h"

struct poisson_timer
{
    soft_timer_t timer;
    double lambda;
    handler_t handler;
    handler_arg_t handler_arg;
    int active;
};

/* Calculate random step before next event in ticks */
unsigned int poisson_step_ticks(double lambda);

/* Start poisson timer with `lambda` distribution */
void poisson_timer_start(struct poisson_timer *ptimer, double lambda,
        handler_t handler, handler_arg_t arg);
/* Start poisson timer */
void poisson_timer_stop(struct poisson_timer *ptimer);

#endif//POISSON_CLOCK_H
