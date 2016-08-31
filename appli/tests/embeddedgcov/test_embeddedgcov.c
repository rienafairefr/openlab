#include "platform.h"

#include "printf.h"
#include "debug.h"
#include "embeddedgcov.h"

#include "max_int.h"

EMBEDDEDGCOV_ARRAY(2);
EMBEDDEDGCOV_BUFFER(512);

#define ASSERT(exp)\
    do {\
        if(!(exp))\
        {\
            log_error("ASSERT(%s) failed at line %d", #exp, __LINE__);\
        }\
    }\
    while(0);

static void test_max_int()
{
    ASSERT(max_int(0, 0) == 0);
    ASSERT(max_int(0, 5) == 5);
    ASSERT(max_int(3, 2) == 3);

    ASSERT(max_int(-3, -2) == -2);
    ASSERT(max_int(-1, -20) == -1);
    ASSERT(max_int(-1, 1) == 1);
}

static void test_app(void *arg)
{
    (void)arg;
    embeddedgcov_init();
    log_info("Running tests");
    test_max_int();
    log_info("Tests finished");
    embeddedgcov_exit();

    vTaskDelete(NULL);
}

int main()
{
    platform_init();
    soft_timer_init();

    // Create a task for the application
    xTaskCreate(test_app, (signed char *) "test_iotlab_time",
            4 * configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    platform_run();

    return 0;
}
