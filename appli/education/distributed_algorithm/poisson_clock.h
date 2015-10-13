#ifndef POISSON_CLOCK_H
#define POISSON_CLOCK_H


/* Calculate random step before next event in ticks */
unsigned int poisson_step_ticks(double lambda);

#endif//POISSON_CLOCK_H
