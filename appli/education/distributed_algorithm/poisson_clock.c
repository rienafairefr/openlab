#include <stdlib.h>
#include "config.h"
#include "random.h"
#include "soft_timer_delay.h"
#include "poisson_clock.h"



// STEP 1 tick
#define STEP 1

// Required to multiply by two...
// I think it comes form the fact that
//     one second == 2^15ticks and random is on 2^16
// It might correct the probability
#define MAGIC 2

unsigned int poisson_delay_in_ticks(double lambda)
{
    double p = lambda * STEP * MAGIC;
    unsigned int delay = 0;

    while (random_rand16() >= p)
        delay += STEP;

    return delay;
}



int poisson_delay(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    double lambda = atof(argv[1]);
    unsigned int delay = poisson_delay_in_ticks(lambda);

    float delay_s = (float)delay / (float)SOFT_TIMER_FREQUENCY;
    MSG("PoissonDelay;%f\n", delay_s);
    return 0;
}
