#include <string.h>

const char *strrchr(const char *s, int c) {
    for(const char *ptr = s + strlen(s); s != ptr; ptr--) {
        if((char) c == *ptr)
            return ptr;
    }

    return NULL;
}
