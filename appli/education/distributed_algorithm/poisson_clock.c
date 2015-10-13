#include <stdlib.h>
#include "random.h"
#include "poisson_clock.h"

// STEP 1 tick
#define STEP 1

// Required to multiply by two...
// I think it comes form the fact that
//     one second == 2^15ticks and random is on 2^16
// It might correct the probability
#define MAGIC 2

unsigned int poisson_step_ticks(double lambda)
{
    double p = lambda * STEP * MAGIC;
    unsigned int delay = 0;

    while (random_rand16() >= p)
        delay += STEP;

    return delay;
}
