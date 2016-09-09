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
    _test_extend(0x2135FFFFFFFFull, 0x0000213600000000ull );
}

static void test_convert_stability()
{
    struct soft_timer_timeval t1, t2, t3, t4, t0_ref;
    struct soft_timer_timeval t1_after, t2_after, t3_after, t4_after, t_utc_update;

    /* First set_time to initialize test time */
    uint64_t t0 = soft_timer_time_64();
    iotlab_time_convert(&t0_ref, t0);

    iotlab_time_set_time(t0, &t0_ref);
    soft_timer_delay_us(5000);

    /* Save a clock tick every 2 ms */
    uint64_t time1 = soft_timer_time_64();
    soft_timer_delay_us(2000);
    uint64_t time2 = soft_timer_time_64();
    soft_timer_delay_us(2000);
    uint64_t time3 = soft_timer_time_64();
    soft_timer_delay_us(2000);
    uint64_t time4 = soft_timer_time_64();
    soft_timer_delay_us(2000);

    /* Convert clock tick to real time */
    iotlab_time_convert(&t1, time1);
    iotlab_time_convert(&t2, time2);
    iotlab_time_convert(&t3, time3);
    iotlab_time_convert(&t4, time4);

    /* New set_time */
    uint64_t t_update = soft_timer_time_64();
    iotlab_time_convert(&t_utc_update, t_update + 10000);
    soft_timer_delay_ms(1);
    iotlab_time_set_time(t_update, &t_utc_update);

    /* Convert old ticks after new set time */
    iotlab_time_convert(&t1_after, time1);
    iotlab_time_convert(&t2_after, time2);
    iotlab_time_convert(&t3_after, time3);
    iotlab_time_convert(&t4_after, time4);

    /* Check that conversion of the ticks before and after the last set_time are equal */
    ASSERT(t1.tv_sec == t1_after.tv_sec);
    ASSERT(t2.tv_sec == t2_after.tv_sec);
    ASSERT(t3.tv_sec == t3_after.tv_sec);
    ASSERT(t4.tv_sec == t4_after.tv_sec);
    ASSERT(t1.tv_usec == t1_after.tv_usec);
    ASSERT(t2.tv_usec == t2_after.tv_usec);
    ASSERT(t3.tv_usec == t3_after.tv_usec);
    ASSERT(t4.tv_usec == t4_after.tv_usec);
}

static void reset_time_config()
{
    int i;
    struct iotlab_time_config null = {0, {0, 0}, 0, SOFT_TIMER_KFREQUENCY_FIX};
    for (i = 0; i < NUM_SAVED_CONFIG + 1; i++)
        time_config[i] = null;
}

static void test_time_convert()
{
    reset_time_config();

    /* Initialize set_time */
    uint64_t t0 = soft_timer_time_64();
    struct soft_timer_timeval time_ref = {1471960848, 400000};
    struct soft_timer_timeval t1_utc;
    iotlab_time_set_time(t0, &time_ref);

    uint64_t t1 = 0;
    iotlab_time_convert(&t1_utc, t1);

    /* Check that for a tick count of 0, the conversion is 0,0 */
    ASSERT(t1_utc.tv_sec == 0);
    ASSERT(t1_utc.tv_usec == 0);
}


static void test_app(void *arg)
{
    (void)arg;
    log_info("Running tests");

    test_time_extend();
    test_convert_stability();
    test_time_convert();

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
