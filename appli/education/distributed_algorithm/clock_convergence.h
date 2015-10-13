#ifndef CLOCK_CONVERGENCE
#define CLOCK_CONVERGENCE

double clock_convergence_system_time(void);
void clock_convergence_init(double time_scale, double time_scale_random,
        double time_offset_random);

int clock_convergence_start(int argc, char **argv);
int clock_convergence_stop(int argc, char **argv);

#endif//CLOCK_CONVERGENCE
