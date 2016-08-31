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

#include "gcov_.h"
#include "gcov.h"
#include <stdio.h>
#include <stdlib.h>

#include "embeddedgcov.h"

/* Ignore compiler warning for these */
void __attribute__((weak)) _init() {}
void __attribute__((weak)) _fini() {}

extern void (*__init_array)(void);
extern void (*__init_array_e)(void);


typedef void (*init_fct_t)(void);

typedef struct tagGcovInfo {
    struct gcov_info *info;
    struct tagGcovInfo *next;
} GcovInfo;
GcovInfo *headGcov = NULL;


int embeddedgcov_init()
{
    /* Call all init functions */
    init_fct_t *init_fct = NULL;
    for (init_fct = &__init_array; init_fct < &__init_array_e; init_fct++) {
        (*init_fct)();
    }
    return 0;
}


/*
 * __gcov_init is called by gcc-generated constructor code for each object
 * file compiled with -fprofile-arcs.
 */
void __gcov_init(struct gcov_info *info)
{
    printf(
        "__gcov_init called for %s!\n",
        gcov_info_filename(info));
    fflush(stdout);
    GcovInfo *newHead = malloc(sizeof(GcovInfo));
    if (!newHead) {
        puts("Out of memory!");
        exit(1);
    }
    newHead->info = info;
    newHead->next = headGcov;
    headGcov = newHead;
}

static void __gcov_exit(struct gcov_info *info);
void embeddedgcov_exit()
{
    GcovInfo *tmp = headGcov;
    while (tmp) {
        __gcov_exit(tmp->info);
        tmp = tmp->next;
    }
}

static void __gcov_exit(struct gcov_info *info)
{
    char *buffer;
    unsigned bytesNeeded = convert_to_gcda(NULL, info);
    buffer = malloc(bytesNeeded);
    if (!buffer) {
        puts("Out of memory!");
        exit(1);
    }
    convert_to_gcda(buffer, info);
    printf("Emitting %6d bytes for %s\n", bytesNeeded, gcov_info_filename(info));
    free(buffer);
}

void __gcov_merge_add(gcov_type *counters, unsigned int n_counters)
{
    puts("__gcov_merge_add isn't called, right? Right? RIGHT?");
    fflush(stdout);
    exit(1);
}
