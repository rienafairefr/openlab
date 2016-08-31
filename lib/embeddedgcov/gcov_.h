#ifndef __GCOV__H_
#define __GCOV__H_

struct gcov_info;
typedef long long gcov_type;

unsigned int convert_to_gcda(char *buffer, struct gcov_info *info);

#endif//__GCOV__H_
