#include <string.h>

void *memset(void *b, int c, size_t len) {
    unsigned char *dst = b;

    while(len--)
        *dst++ = (unsigned char) c;

    return b;
}