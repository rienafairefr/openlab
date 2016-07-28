#include "platform.h"

#include <string.h>

#include "printf.h"
#include "debug.h"

// Include C file for tests
#include "iotlab_time.c"

#define ASSERT(exp)\
    do {\
        if(!(exp))\
        {\
            log_error("ASSERT(%s) failed at line %d", #exp, __LINE__);\
        }\
    }\
    while(0);

static char *str_uint64b(uint64_t time)
{
    static char buf[32];
    uint32_t high = (uint32_t) ((time >> 32) & 0xFFFFFFFF);
    uint32_t low = (uint32_t) (time & 0xFFFFFFFF);
    sprintf(buf, "0x%08x%08x", high, low);
    return buf;
}

#define DEBUG_TIME 1
static void print64(char *fmt, uint64_t time)
{
#if DEBUG_TIME
    printf(fmt, str_uint64b(time));
#endif//DEBUG_TIME
}



/*
 * Verify that for a given 64b time, if it's converted back to a 32bit
 * The extended time of this 32bit has the same value as the original 64b
 */
#define _test_extend(time, time64)         do {             \
    /* Get 'original' 32b time */                           \
    uint32_t time32 = (uint32_t) (time & 0xFFFFFFFF);       \
                                                            \
    /* Time to extend cannot be greater than time64 */      \
    ASSERT((uint64_t)time <= time64);                       \
                                                            \
    uint64_t extended = 0;                                  \
                                                            \
    extended = get_extended_time(time32, time64);           \
    ASSERT(time == extended);                               \
    print64("time32   %s\n", time32);                       \
    print64("time64   %s\n", time64);                       \
    print64("extended %s\n", extended);                     \
    print64("expected %s\n", time);                         \
    printf("\n");                                           \
} while (0)


static void test_time_extend()
{
    _test_extend(0xABCD1234ull,     0x00000000ABCDAAAAull);
    _test_extend(0xFFFF1234ull,     0x00000000FFFFAAAAull);
    _test_extend(0xFFFF1234ull,     0x000000010000ABCDull);

    /* Test with bigger uint64 */
    _test_extend(0x1234ABCD1234ull, 0x00001234ABCDAAAAull);
    _test_extend(0x1234FFFF1234ull, 0x00001234FFFFAAAAull);
    _test_extend(0x1234FFFF1234ull, 0x000012350000AAAAull);

    /* This should not overflow */
    _test_extend(0x12347FFF1234ull, 0x000012348000AAAAull);

    // TODO add other tests...
}


static void test_app(void *arg)
{
    (void)arg;
    log_info("Running tests");

    test_time_extend();

    log_info("Tests finished");

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
