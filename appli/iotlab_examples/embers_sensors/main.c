#include <stdint.h>
#include <stdlib.h>
#include <printf.h>
#include <string.h>
#include <time.h>
#include "scanf.h"

#include <platform.h>
#include "shell.h"
#include "soft_timer.h"

#ifdef IOTLAB_M3
#include "lps331ap.h"
#include "isl29020.h"
#endif

static soft_timer_t sensors_timer;

// 5 seconds
#define TIMER_DEFAULT_DELAY    5000

#ifdef IOTLAB_M3
/**
 * Sensors
 */
static void print_sensors(handler_arg_t arg)
{
    int16_t temp;
    lps331ap_read_temp(&temp);
    float lum = isl29020_read_sample();
    uint32_t pres;
    lps331ap_read_pres(&pres);
    printf("{\"temperature\":%f,\"luminosity\":%f, \"pressure\":%f\"}\n", 42.5 + temp / 480.0, lum, pres / 4096.0 );
    //printf("Chip temperature measure: %f\n", 42.5 + temp / 480.0);
    //printf("Luminosity measure: %f lux\n", light);
    //printf("Pressure measure: %f mabar\n", pres / 4096.0); 
}
#endif

static int sensors_on(int argc, char **argv)
{
    uint16_t delay = TIMER_DEFAULT_DELAY;
    if (argc == 2)
    	if (1 != sscanf(argv[1], "%u", &delay))
            return 1;
    soft_timer_start(&sensors_timer, soft_timer_ms_to_ticks(delay), 1);
    return 0;
}

static int sensors_off(int argc, char **argv)
{
    soft_timer_stop(&sensors_timer);
    return 0;
}

static void hardware_init()
{
    // Openlab platform init
    platform_init();
    event_init();
    soft_timer_init();
    soft_timer_set_handler(&sensors_timer, print_sensors, NULL);

    // Switch off the LEDs
    leds_off(LED_0 | LED_1 | LED_2);

#ifdef IOTLAB_M3
    // ISL29020 light sensor initialisation
    isl29020_prepare(ISL29020_LIGHT__AMBIENT, ISL29020_RESOLUTION__16bit,
            ISL29020_RANGE__16000lux);
    isl29020_sample_continuous();

    // LPS331AP pressure sensor initialisation
    lps331ap_powerdown();
    lps331ap_set_datarate(LPS331AP_P_12_5HZ_T_12_5HZ);
#endif

}

struct shell_command commands[] = {
    {"sensors_on",          "Start sensors measure",              sensors_on},
    {"sensors_off",         "Stop sensors measure",              sensors_off},
    {NULL, NULL, NULL},
};

int main()
{
    hardware_init();
    shell_init(commands, 0);
    platform_run();
    return 0;
}
