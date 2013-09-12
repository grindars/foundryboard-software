#ifndef __VCPRINTF_P__H__
#define __VCPRINTF_P__H__

typedef enum {
        LenDefault,
        LenChar,
        LenShort,
        LenInt,
        LenLong,
#ifdef PRINTF_LONGLONG
        LenLongLong,
#endif
        LenSize,
        LenPtrdiff,
        LenPointer
} length_t;

#ifdef PRINTF_LONGLONG
typedef unsigned long long printf_uintmax_t;
typedef long long printf_intmax_t;
#else
typedef unsigned long printf_uintmax_t;
typedef long printf_intmax_t;
#endif

typedef struct {
        unsigned int alternate     : 1;
        unsigned int zero_padding  : 1;
        unsigned int left_adjusted : 1;
        unsigned int valid_flag    : 1;
        unsigned int pointer_type  : 1;
        unsigned int no_value     : 1;
        unsigned int is_unsigned   : 1;
        unsigned int upper         : 1;
        unsigned int separator     : 1;

        length_t length;

        char positive_char;
        int field_width;
        int precision;

        union {

                printf_uintmax_t vunsigned;
                printf_intmax_t vsigned;

                void *pointer;

                ptrdiff_t ptrdiff;
                size_t size;

        } value;
} vcprintf_flags_t;

typedef struct {
        int base;
        vcprintf_callback_t callback;
        void *pass;

        int highest;

        unsigned int upper: 1;
        unsigned int separator     : 1;
} putnum_settings_t;

#endif


