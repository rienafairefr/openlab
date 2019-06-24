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
#include "i2c_slave.h"

#include "queue.h"

extern void i2c_slave_start();

void hardware_init()
{
	platform_init();

	soft_timer_init();

	i2c_slave_start();
}


int main()
{
	// Initialize the platform
    hardware_init();
	// Run
	platform_run();
	return 0;
}
