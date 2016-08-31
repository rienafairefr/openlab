#include "max_int.h"

int max_int(int a, int b)
{
    if (b == a) {
        return a;
    } else
        if (b < a) {
        return a;
    } else {
        return b;
    }
}

