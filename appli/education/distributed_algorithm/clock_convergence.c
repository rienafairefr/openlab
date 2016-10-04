#include <string.h>
#include <stdlib.h>
#include "random.h"
#include "soft_timer_delay.h"
#include "poisson_clock.h"
#include "radio_network.h"
#include "clock_convergence.h"
#include "config.h"

#define RAND_DOUBLE() (random_rand16() / ((float)(1<<16)))
#define TIMER_TIME() (((double)soft_timer_time()) / SOFT_TIMER_FREQUENCY)


struct clock_pkt {
    uint8_t type;
    double time;
    uint32_t num_wakeup;
};


static struct {
    struct poisson_timer timer;
    uint32_t num_wakeup;
    struct {
        double scale;
        double offset;
        double time_0;
    } system_t;
    double lambda;
    double t_sys_up;
    double t_virt_up;
    double k_up;
} cfg;


double clock_convergence_virtual_time()
{
    double t_sys = clock_convergence_system_time();

    // Use cfg.k_up, cfg.t_virt_up and cfg.t_sys_up here
    double t_virt = t_sys;

    return t_virt;
}


static void clock_convergence_update_time(double neigh_virtual_time)
{
    double t_virt = clock_convergence_virtual_time();
    double t_sys = clock_convergence_system_time();
    double alpha = 0; // Configure me
    double q = 0;     // configure me

    /* Hide compile 'error: unused variable' */
    (void) neigh_virtual_time;
    (void) t_virt;
    (void) t_sys;
    (void) alpha;
    (void) q;

    /* // Update values
     *   cfg.t_sys_up =
     *   cfg.t_virt_up =
     *   cfg.k_up =
     */

}


static void clock_convergence_print_time()
{
    MSG("Clock;%f;%f\n", clock_convergence_system_time(),
            clock_convergence_virtual_time());
}


void clock_convergence_handle_time(uint16_t src_addr, const uint8_t *data,
        size_t length)
{
    if (sizeof(struct clock_pkt) != length)
        ERROR("Invalid Clock pkt len\n");

    struct clock_pkt pkt;
    memcpy(&pkt, data, sizeof(struct clock_pkt));

    clock_convergence_print_time();
    clock_convergence_update_time(pkt.time);
    clock_convergence_print_time();
}


double clock_convergence_system_time()
{
    double time = TIMER_TIME();
    time = (time - cfg.system_t.time_0);
    time *= cfg.system_t.scale;
    time += cfg.system_t.offset;
    return time;
}


void clock_convergence_init(double time_scale, double time_scale_random,
        double time_offset_random)
{
    cfg.system_t.time_0 = TIMER_TIME();
    cfg.system_t.offset = time_offset_random * RAND_DOUBLE();
    cfg.system_t.scale = time_scale + time_scale_random * RAND_DOUBLE();
}

/* Timer handler */
static void clock_convergence_handler(handler_arg_t arg)
{
    // Send time to neighbours

    struct clock_pkt pkt;
    pkt.type = PKT_CLOCK;
    pkt.time = clock_convergence_virtual_time();
    pkt.num_wakeup = ++cfg.num_wakeup;

    network_send_no_retry(&pkt, sizeof(struct clock_pkt));
}


int clock_convergence_start(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    double lambda = atof(argv[1]);
    cfg.lambda = lambda;

    cfg.num_wakeup = 0;

    // We want divergence, not base offset
    cfg.system_t.time_0 = TIMER_TIME();

    // Init our time
    cfg.t_sys_up = clock_convergence_system_time();
    cfg.t_virt_up = cfg.t_sys_up;
    cfg.k_up = 1;

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
