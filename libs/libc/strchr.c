#include <string.h>

char *strchr(const char *s, int c) {
    while(*s != 0) {
        if(*s == (char) c)
            return (char *) s;

        s++;
    }

    return NULL;
}