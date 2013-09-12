#include <string.h>

void *memcpy(void *restrict s1, const void *restrict s2, size_t n) {
    unsigned char *restrict bytedst = s1;
    const unsigned char *restrict bytesrc = s2;

    while(n--)
        *bytedst++ = *bytesrc++;

    return s1;
}