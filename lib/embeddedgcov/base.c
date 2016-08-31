/*
 * This code is based on the GCOV-related code of the Linux kernel (kept under
 * "kernel/gcov"). It basically uses the convert_to_gcda function to generate
 * the .gcda files information upon application completion, and dump it on the
 * host filesystem via GDB scripting.
 *
 * Original Linux banner follows below - but note that the Linux guys have
 * nothing to do with these modifications, so blame me (and contact me)
 * if something goes wrong.
 *
 * Thanassis Tsiodras
 * Real-time Embedded Software Engineer
 * System, Software and Technology Department
 * European Space Agency
 *
 * e-mail: ttsiodras@gmail.com / Thanassis.Tsiodras@esa.int (work)
 *
 */


/*
 *  This code maintains a list of active profiling data structures.
 *
 *    Copyright IBM Corp. 2009
 *    Author(s): Peter Oberparleiter <oberpar@linux.vnet.ibm.com>
 *
 *    Uses gcc-internal data definitions.
 *    Based on the gcov-kernel patch by:
 *             Hubertus Franke <frankeh@us.ibm.com>
 *             Nigel Hinds <nhinds@us.ibm.com>
 *             Rajan Ravindran <rajancr@us.ibm.com>
 *             Peter Oberparleiter <oberpar@linux.vnet.ibm.com>
 *             Paul Larson
 */

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "gcov_.h"
#include "gcov.h"

#include "embeddedgcov.h"

/* Ignore compiler warning for these */
void __attribute__((weak)) _init() {}
void __attribute__((weak)) _fini() {}

extern void (*__init_array)(void);
extern void (*__init_array_e)(void);


typedef void (*init_fct_t)(void);

#ifndef GCOV_ARRAY_SIZE
#define GCOV_ARRAY_SIZE (8)
#endif//GCOV_ARRAY_SIZE
#ifndef GCOV_BUFFER_SIZE
#define GCOV_BUFFER_SIZE (256)
#endif//GCOV_BUFFER_SIZE

/*
 * Weak default values
 */
/* Gcov struct pointer array */
size_t           __attribute__((weak))  embeddedgcov_array_size = GCOV_ARRAY_SIZE;
struct gcov_info __attribute__((weak)) *embeddedgcov_array[GCOV_ARRAY_SIZE];
/* Gcov gcda buffer */
size_t           __attribute__((weak))  embeddedgcov_buffer_size = GCOV_BUFFER_SIZE;
char             __attribute__((weak))  embeddedgcov_buffer[GCOV_BUFFER_SIZE];

static size_t gcov_cur_index = 0;
static size_t gcov_max_index = 0;

static unsigned _embeddedgcov_max_gcda_size();

int embeddedgcov_init()
{
    /* Reset array */
    gcov_max_index = 0;
    gcov_cur_index = 0;

    /* Call all init functions */
    init_fct_t *init_fct = NULL;
    for (init_fct = &__init_array; init_fct < &__init_array_e; init_fct++) {
        (*init_fct)();
    }

    /* Verify allocated buffer size */
    unsigned max_gcda_size = _embeddedgcov_max_gcda_size();
    if (embeddedgcov_buffer_size < max_gcda_size) {
        log_error("embeddedgcov_buffer_size < max gcda size: %u < %u",
                embeddedgcov_buffer_size, max_gcda_size);
        log_error("Some files will not be covered");
    }

    return gcov_max_index;
}

/*
 * __gcov_init is called by gcc-generated constructor code for each object
 * file compiled with -fprofile-arcs.
 */
void __gcov_init(struct gcov_info *info)
{
    size_t index = gcov_cur_index++;


    log_debug("__gcov_init called for %s", gcov_info_filename(info));
    if (index < embeddedgcov_array_size) {
        embeddedgcov_array[index] = info;
        gcov_max_index++;
        return;
    }

    log_error("embeddedgcov_array too small %u > %u ignore %s",
            index + 1, embeddedgcov_array_size,
            gcov_info_filename(info));
}


static void __gcov_exit(struct gcov_info *info);
void embeddedgcov_exit()
{
    int i;
    for (i = 0; i < gcov_max_index; i++) {
        __gcov_exit(embeddedgcov_array[i]);
    }
}

void embeddedgcov_export_gcda(const char *filename, char *buffer, size_t size)
{
    (void)buffer;
    printf("Emitting %6d bytes for %s\n", gcda_size, filename);
}

static void __gcov_exit(struct gcov_info *info)
{
    /* Verify allocated size */
    unsigned gcda_size = convert_to_gcda(NULL, info);
    if (embeddedgcov_buffer_size < gcda_size) {
        log_error("embeddedgcov_buffer_size too small to save gcda %u < %u."
                " Ignoring %s",
                embeddedgcov_buffer_size, gcda_size,
                gcov_info_filename(info));
        return;
    }

    /* Generate gcda data */
    convert_to_gcda(embeddedgcov_buffer, info);

    /* Export gcda data */
    embeddedgcov_export_gcda(gcov_info_filename(info),
            embeddedgcov_buffer, gcda_size);
}

void __gcov_merge_add(gcov_type *counters, unsigned int n_counters)
{
    log_error("__gcov_merge_add isn't called, right? Right? RIGHT?");
    exit(1);
}

#define MAX(a,b) \
    ({ \
     __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b;       \
     })
static unsigned _embeddedgcov_max_gcda_size()
{
    unsigned buffer_size = 0;
    int i;
    for (i = 0; i < gcov_max_index; i++) {
        unsigned bytes_needed = convert_to_gcda(NULL, embeddedgcov_array[i]);
        buffer_size = MAX(buffer_size, bytes_needed);
    }
    return buffer_size;
}
