#include <stdint.h>
#include <stdlib.h>
#include <printf.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "scanf.h"
#include "random.h"

#include <platform.h>
#include "shell.h"
#include "soft_timer.h"
#include "iotlab_leds.h"

#include "lps331ap.h"
#include "isl29020.h"

struct custom_timer
{
    soft_timer_t timer;
    int active;
    unsigned int param_delay;
    int (*handler)(handler_arg_t);
};

// 5 seconds
#define TIMER_DEFAULT_DELAY_SECONDS    5
static int sensors_handler(handler_arg_t arg);
#define PARKING_DEFAULT_DELAY_SECONDS 30
static int parking_busy;
static int parking_handler(handler_arg_t arg);


static int has_random_at_start = 1;
static struct custom_timer sensors_timer = {
    .active = 0, .handler = sensors_handler};
static struct custom_timer parking_timer = {
    .active = 0, .handler = parking_handler};


static void timer_handler(handler_arg_t arg)
{
    struct custom_timer *timer = (struct custom_timer *)arg;
    if (!timer->active)
        return;

    uint32_t next = timer->handler(timer);
    soft_timer_start(&timer->timer, next, 0);
}


/**
 * Sensors
 */
static int sensors_handler(handler_arg_t arg)
{
    struct custom_timer *timer = (struct custom_timer *)arg;

    int16_t temp;
    lps331ap_read_temp(&temp);
    float lum = isl29020_read_sample();
    uint32_t pres;
    lps331ap_read_pres(&pres);

    printf("{");
    printf("\"device_type\":\"sensor\",");
    printf("\"name\":\"environment_event\",");
    printf("\"temperature\":%f,", 42.5 + temp / 480.0);
    printf("\"light\":%f,", lum);
    printf("\"pressure\":%f", pres / 4096.0 ); // No comma at the end
    printf("}\n");

    return soft_timer_s_to_ticks(timer->param_delay);
}


unsigned poisson_step_ticks(unsigned int inv_lambda)
{
    double rand = random_rand16() / ((float)(1<<16));
    double delay;

    delay = -log(rand) * inv_lambda;
    // seconds to ticks for floats
    delay *= SOFT_TIMER_FREQUENCY;  // in seconds

    return delay;
}

static void set_parking_led()
{
    if (parking_busy)
        leds_on(GREEN_LED);
    else
        leds_off(GREEN_LED);
}

static int parking_handler(handler_arg_t arg)
{
    struct custom_timer *timer = (struct custom_timer *)arg;

    parking_busy = !parking_busy;
    set_parking_led();
    printf("{");
    printf("\"device_type\":\"sensor\",");
    printf("\"name\":\"parking_event\",");
    printf("\"status\":%d", (parking_busy ? 1: 0));
    printf("}\n");
    return poisson_step_ticks(timer->param_delay);
}


static unsigned random_delay_if_on(unsigned max_seconds)
{
    if (!has_random_at_start)
        return 0;

    unsigned rand = random_rand32();
    return rand % soft_timer_s_to_ticks(max_seconds);
}

static int sensors_on(int argc, char **argv)
{
    uint16_t delay = TIMER_DEFAULT_DELAY_SECONDS;
    if (argc == 2) {
        if (1 != sscanf(argv[1], "%u", &delay))
            return 1;
        if (delay == 0)
            return 1;
    }

    sensors_timer.param_delay = delay;
    sensors_timer.active = 1;

    unsigned first = random_delay_if_on(sensors_timer.param_delay);
    soft_timer_start(&sensors_timer.timer, first, 0);

    return 0;
}


static int sensors_off(int argc, char **argv)
{
    sensors_timer.active = 0;
    return 0;
}


static int parking_on(int argc, char **argv)
{
    uint16_t delay = PARKING_DEFAULT_DELAY_SECONDS;
    if (argc == 2) {
        if (1 != sscanf(argv[1], "%u", &delay))
            return 1;
        if (delay == 0)
            return 1;
    }

    parking_timer.param_delay = delay;
    parking_timer.active = 1;

    unsigned first = random_delay_if_on(parking_timer.param_delay);
    soft_timer_start(&parking_timer.timer, first, 0);

    return 0;
}

static int parking_off(int argc, char **argv)
{
    parking_timer.active = 0;
    return 0;
}

static int traffic_echo(int argc, char **argv)
{
    uint16_t vehicle_count;
    if (argc != 2)
        return 1;
    if (1 != sscanf(argv[1], "%u", &vehicle_count))
        return 1;

    printf("{");
    printf("\"device_type\":\"sensor\",");
    printf("\"name\":\"traffic_event\",");
    printf("\"count\":%d", vehicle_count);
    printf("}\n");
    
    return 0;
}

static int pollution_echo(int argc, char **argv)
{
    float ozone, particles, carbon_o, sulfure_o2, nitrogen_o2;
    if (argc != 2)
        return 1;
    if (5 != sscanf(argv[1], "%f,%f,%f,%f,%f",
                 &ozone, &particles, &carbon_o, &sulfure_o2, &nitrogen_o2))
        return 1;

    printf("{");
    printf("\"device_type\":\"sensor\",");
    printf("\"name\":\"pollution_event\",");
    printf("\"ozone\":%f,", ozone);
    printf("\"particles\":%f,", particles);
    printf("\"carbon_monoxide\":%f,", carbon_o);
    printf("\"sulfure_dioxide\":%f,", sulfure_o2);
    printf("\"nitrogen_dioxide\":%f", nitrogen_o2);
    printf("}\n");

    return 0;
}

static int random_on(int argc, char **argv)
{
    has_random_at_start = 1;
    return 0;
}
static int random_off(int argc, char **argv)
{
    has_random_at_start = 0;
    return 0;
}

static void hardware_init()
{
    // Openlab platform init
    platform_init();

    // Switch off the LEDs
    leds_off(LED_0 | LED_1 | LED_2);

    // ISL29020 light sensor initialisation
    isl29020_prepare(ISL29020_LIGHT__AMBIENT, ISL29020_RESOLUTION__16bit,
            ISL29020_RANGE__16000lux);
    isl29020_sample_continuous();

    // LPS331AP pressure sensor initialisation
    lps331ap_powerdown();
    lps331ap_set_datarate(LPS331AP_P_12_5HZ_T_12_5HZ);

    parking_busy = random_rand32() % 1;
    set_parking_led();
}

static void timer_init()
{
    event_init();
    soft_timer_init();
    soft_timer_set_handler(&sensors_timer.timer, timer_handler, &sensors_timer);
    soft_timer_set_handler(&parking_timer.timer, timer_handler, &parking_timer);
}

struct shell_command commands[] = {
    {"sensors_on",          "[delay:seconds] Start sensors measure. Default 5s, min 1s", sensors_on},
    {"sensors_off",         "Stop sensors measure",              sensors_off},
    {"parking_on",          "[average delay:seconds] Start parking simulator. Default 30s, min 1s", parking_on},
    {"parking_off",         "Stop parking simulator",              parking_off},
    {"traffic",             "[vehicle count]  Send traffic event",                     traffic_echo},
    {"pollution",           "[o2,particles,co,so2,no2]  Send pollution event",         pollution_echo},
    {"random_on",           "Add a random delay before starting measures, default ON", random_on},
    {"random_off",          "No random delay before starting measures.",               random_off},
    {NULL, NULL, NULL},
};

int main()
{
    hardware_init();
    shell_init(commands, 0);
    timer_init();
    platform_run();
    return 0;
}
