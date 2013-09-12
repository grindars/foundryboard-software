#include <string.h>

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *cs1 = s1, *cs2 = s2;

    while(n--) {
        if(*cs1 != *cs2)
            break;

        cs1++;
        cs2++;
    }

    return *cs1 - *cs2;
}
