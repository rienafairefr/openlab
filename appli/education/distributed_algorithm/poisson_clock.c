#include <stdlib.h>
#include <math.h>
#include "soft_timer.h"
#include "random.h"
#include "poisson_clock.h"


unsigned int poisson_step_ticks(double lambda)
{
    double rand = random_rand16() / ((float)(1<<16));
    double delay;

    delay = -log(rand) / lambda;
    delay *= SOFT_TIMER_FREQUENCY;  // in seconds

    return delay;
}

/* Timer is set `periodic` manually. */
static void poisson_timer_schedule(struct poisson_timer *ptimer)
{
    uint32_t ticks = poisson_step_ticks(ptimer->lambda);
    soft_timer_start(&ptimer->timer, ticks, 0);
}

/* Call handler and restart timer
 *
 * The `active` state solves concurrency issues (stop while handler is
 * already on the event queue).
 *
 * `periodic` is handled manually by re-scheduling next event
 *
 */
static void poisson_timer_handler(handler_arg_t arg)
{
    struct poisson_timer *ptimer = (struct poisson_timer *)arg;
    if (!ptimer->active)
        return;

    ptimer->handler(ptimer->handler_arg);
    poisson_timer_schedule(ptimer);
}

void poisson_timer_start(struct poisson_timer *ptimer, double lambda,
        handler_t handler, handler_arg_t arg)
{
    soft_timer_set_handler(&ptimer->timer,
            poisson_timer_handler, (handler_arg_t)ptimer);
    ptimer->lambda      = lambda;
    ptimer->handler     = handler;
    ptimer->handler_arg = arg;
    ptimer->active      = 1;

    poisson_timer_schedule(ptimer);
}

void poisson_timer_stop(struct poisson_timer *ptimer)
{
    ptimer->active = 0;
    soft_timer_stop(&ptimer->timer);
}
