#ifndef __EMBEDDEDGCOV_PUBLIC_H__
#define __EMBEDDEDGCOV_PUBLIC_H__

#include <stdlib.h>
struct gcov_info;


/** EMBEDDEDGCOV_ARRAY create embeddedgcov array.
 * Overrides default 'weak' declaration
 * \param size  size of array should be greater than number of files
 */
#define EMBEDDEDGCOV_ARRAY(size) \
    size_t            embeddedgcov_array_size = (size); \
    struct gcov_info *embeddedgcov_array[size];


/** EMBEDDEDGCOV_BUFFER create embeddedgcov gcda buffer.
 * Overrides default 'weak' declaration
 * \param size  size of array should be greater than biggest gcda file.
 */
#define EMBEDDEDGCOV_BUFFER(size) \
    size_t embeddedgcov_buffer_size = (size); \
    char   embeddedgcov_buffer[size];


int embeddedgcov_init();
void embeddedgcov_exit();

#endif//_EMBEDDEDGCOV_PUBLIC_H__
