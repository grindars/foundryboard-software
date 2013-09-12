#include <stdlib.h>

static int ParseDigit(char Character, int Base) {
    int Result;

    if(Character >= 'a' && Character <= 'z')
        Result = Character - 'a' + 10;
    else if(Character >= 'A' && Character <= 'Z')
        Result = Character - 'A' + 10;
    else if(Character >= '0' && Character <= '9')
        Result = Character - '0';
    else
        return -1;

    if(Result >= Base)
        return -1;
    else
        return Result;
}

long strtol(const char *restrict String, char **End, int Base) {
    long Number = 0;
    int Digit;

    if(Base == 0 && String[0] == '0' && String[1] == 'x')
        Base = 16;
    else if(Base == 0 && String[0] == '0')
        Base = 8;
    else if(Base == 0)
        Base = 10;

    if(Base == 16 && String[0] == '0' && String[1] == 'x')
        String += 2;

    if(Base == 8 && String[0] == '0')
        String++;

    for(; *String; String++) {
        Digit = ParseDigit(*String, Base);

        if(Digit == -1)
            break;

        Number = Number * Base + Digit;
    }

    if(End)
        *End = (char *) String;

    return Number;
}
