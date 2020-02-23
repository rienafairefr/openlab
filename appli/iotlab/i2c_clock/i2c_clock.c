/*
 * I2C clock A8-M3
 *
 */

#include <stdint.h>
#include <math.h>
#include <string.h>

#include "platform.h"

#include "printf.h"
#include "soft_timer.h"
#include "soft_timer_delay.h"
#include "iotlab_i2c.h"

#include "queue.h"


void hardware_init()
{
	platform_init();
	iotlab_i2c_init();
}


int main()
{
	// Initialize the platform
    hardware_init();
	// Run
	platform_run();
	return 0;
}


static void app_task(void *param)
{
	printf("STARTING\n");

	while (1) {
		echo_line();
	}
}



