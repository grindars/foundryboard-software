// TODO:
// * Positional argument support
// * Floating point support
// * Refactor it!

// Extensions: (enabled by PRINTF_EXTENSIONS)
// * Grouping hexadecimal numbers by _ (enabled by flag ')

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#undef PRINTF_EXTENSIONS
#undef PRINTF_LONGLONG

#include "vcprintf_p.h"

static int putnum(printf_uintmax_t absolute, putnum_settings_t *set, int precision) {
        char buf[64];

        int ret = 0;

        int put_digits = 0;

#ifdef PRINTF_EXTENSIONS
        int per_digits = 0;
        char separator = 0;

        if(set->separator) {
                switch(set->base) {
                case 10:
                        per_digits = 3;
                        separator = 0x27;

                        break;

                case 16:
                        per_digits = 4;
                        separator = '_';

                        break;
                }
        }
#endif

        while(absolute > 0 || precision > 0) {
                set->highest = absolute % set->base;

#ifdef PRINTF_EXTENSIONS
                if(per_digits != 0 && put_digits == per_digits) {
                        buf[ret++] = separator;
#else
                if(set->separator && put_digits == 3) {
                        buf[ret++] = 0x27;
#endif
                        put_digits = 0;
                }

                char ch;

                if(set->highest >= 10)
                    ch = 'a' + set->highest - 10;
                else
                    ch = '0' + set->highest;

                if(set->upper && ch >= 'a' && ch <= 'z')
                        ch = ch - 'a' + 'A';

                buf[ret] = ch;

                ret++;
                put_digits++;
                absolute /= set->base;
                precision--;
        }

        if(set->callback)
                for(int i = 0; i < ret; i++)
                        set->callback(buf[ret - i - 1], set->pass);

        return ret;
}

static int outnum(vcprintf_flags_t *flags, int base, vcprintf_callback_t callback, void *pass) {
        printf_uintmax_t absolute;
        int negative = 0;
        int bytes = 0;
        char sign = 0;

        if(flags->is_unsigned) {
                absolute = flags->value.vunsigned;
        } else {
                if(flags->value.vsigned < 0) {
                        negative = 1;
                        absolute = -flags->value.vsigned;
                } else
                        absolute = flags->value.vsigned;
        }

        if(negative) {
                sign = '-';
        } else if(absolute > 0) {
                sign = flags->positive_char;
        }

        putnum_settings_t put_set;
        put_set.base = base;
        put_set.callback = 0;
        put_set.pass = 0;
        put_set.upper = flags->upper;
        put_set.separator = flags->separator;

        if(flags->precision == -1)
                flags->precision = 1;

        int required_width = putnum(absolute, &put_set, flags->precision);

        if(sign != 0)
                required_width++;

        char filler = flags->zero_padding ? '0' : ' ';
        int to_fill = flags->field_width - required_width;

        if(required_width < flags->field_width && flags->left_adjusted == 0) {
                bytes += to_fill;

                for(int i = 0; i < to_fill; i++)
                        callback(filler, pass);
        }

        if(flags->alternate && base == 8 && put_set.highest != 0) {
                flags->precision = required_width + 1;
                required_width++;
        }

        if(absolute != 0 || flags->precision != 0) {
                if(sign != 0)
                        callback(sign, pass);

                if(flags->alternate && base == 16) {
                        bytes += 2;

                        callback('0', pass);
                        callback('x', pass);
                }


                put_set.callback = callback;
                put_set.pass = pass;

                putnum(absolute, &put_set, flags->precision);

                bytes += required_width;
        }

        if(required_width < flags->field_width && flags->left_adjusted == 1) {
                bytes += to_fill;

                for(int i = 0; i < to_fill; i++)
                        callback(filler, pass);
        }


        return bytes;
}

static int outs(vcprintf_flags_t *flags, vcprintf_callback_t callback, void *pass) {
        char *str = flags->value.pointer;

        int len = strlen(str), pad = 0;

        if(flags->field_width != -1) {
            if(len > flags->field_width)
                len = flags->field_width;
            else if(len < flags->field_width && flags->left_adjusted == 0) {
                pad = flags->field_width - len;

                for(int i = 0; i < pad; i++)
                    callback(' ', pass);
            }
        }

        for(int i = 0; i < len; i++, str++)
                callback(*str, pass);

        if(flags->field_width != -1 && len < flags->field_width && flags->left_adjusted) {
            pad = flags->field_width - len;

            for(int i = 0; i < pad; i++)
                callback(' ', pass);
        }

        return len + pad;
}

static void place_len(vcprintf_flags_t *flags, int len) {
        switch(flags->length) {
        case LenChar: {
                char *p = flags->value.pointer;

                *p = len;

                break;
        }

        case LenShort: {
                short *p = flags->value.pointer;

                *p = len;

                break;
        }

        case LenInt: {
                int *p = flags->value.pointer;

                *p = len;

                break;
        }

        case LenLong: {
                long *p = flags->value.pointer;

                *p = len;

                break;
        }

#ifdef PRINTF_LONGLONG
        case LenLongLong: {
                long long *p = flags->value.pointer;

                *p = len;

                break;
        }
#endif

        case LenSize: {
                size_t *p = flags->value.pointer;

                *p = len;

                break;
        }

        case LenPtrdiff: {
                ptrdiff_t *p = flags->value.pointer;

                *p = len;

                break;
        }

        default:
                break;
        }
}

int vcprintf(const char *fmt, va_list list, vcprintf_callback_t callback, void *pass) {
        int len = 0;

        for(; *fmt; fmt++) {
                if(*fmt == '%') {
                        fmt++;

                        vcprintf_flags_t flags;
                        memset(&flags, 0, sizeof(flags));

                        flags.precision = -1;
                        flags.field_width = -1;

                        // Flags
                        flags.valid_flag = 1;

                        do {
                                switch(*fmt) {
                                case '#':
                                        flags.alternate = 1;

                                        break;

                                case 0x27: // '
                                        flags.separator = 1;

                                        break;

                                case '0':
                                        flags.zero_padding = 1;

                                        break;

                                case '-':
                                        flags.left_adjusted = 1;

                                        break;

                                case ' ':
                                        if(flags.positive_char == 0)
                                                flags.positive_char = ' ';

                                        break;

                                case '+':
                                        flags.positive_char = '+';

                                        break;

                                case 0: // end of string
                                        return len;

                                default:
                                        flags.valid_flag = 0;
                                }

                                if(flags.valid_flag)
                                        fmt++;

                        } while(flags.valid_flag);

                        // Field width

                        if(*fmt == '*') {
                                flags.field_width = va_arg(list, int);

                                fmt++;
                        } else if(*fmt >= '0' && *fmt <= '9') {
                                flags.field_width = strtol(fmt, (char **) &fmt, 10);
                        }

                        // Precision

                        if(*fmt == '.') {
                                fmt++;

                                if(*fmt == '*') {
                                        flags.precision = va_arg(list, int);

                                        fmt++;
                                } else if(*fmt >= '0' && *fmt <= '9') {
                                        flags.precision = strtol(fmt, (char **) &fmt, 10);

                                        flags.zero_padding = 0;
                                }
                        }

                        // Length modifiers

                        flags.valid_flag = 1;

                        do {
                                switch(*fmt)  {
                                case 'h':
                                        if(flags.length == LenDefault) {
                                                flags.length = LenShort;
                                        } else if(flags.length == LenShort) {
                                                flags.length = LenChar;
                                        } else {
                                                flags.valid_flag = 0;
                                        }

                                        break;

                                case 'l':
                                        if(flags.length == LenDefault) {
                                                flags.length = LenLong;
#ifdef PRINTF_LONGLONG
                                        } else if(flags.length == LenLong) {
                                                flags.length = LenLongLong;
#endif
                                        } else {
                                                flags.valid_flag = 0;
                                        }

                                        break;

#ifdef PRINTF_LONGLONG
                                case 'j':
                                        flags.length = LenLongLong;

                                        break;
#endif
                                case 'z':
                                        flags.length = LenSize;

                                        break;

                                case 't':
                                        flags.length = LenPtrdiff;

                                        break;

                                default:
                                        flags.valid_flag = 0;
                                }

                                if(flags.valid_flag)
                                        fmt++;
                        } while(flags.valid_flag);

                        char conversion = *fmt;

                        // Default length and sign

                        length_t default_length = LenInt;

                        switch(conversion) {
                        case 'd':
                        case 'i':
                                default_length = LenInt;
                                flags.is_unsigned = 0;

                                break;

                        case 'o':
                        case 'u':
                        case 'x':
                        case 'X':
                                default_length = LenInt;
                                flags.is_unsigned = 1;

                                break;

                        case 'c':
                                default_length = LenChar;
                                flags.is_unsigned = 1;

                                break;

                        case 's':
                                default_length = LenPointer;
                                flags.pointer_type = 1;

                                break;

                        case 'p':
                                default_length = LenLong;
                                flags.is_unsigned = 1;

                                break;

                        case 'n':
                                default_length = LenInt;
                                flags.pointer_type = 1;

                                break;

                        case '%':
                                flags.no_value = 1;
                        }

                        if(flags.length == LenDefault)
                                flags.length = default_length;

                        // Value

                        if(flags.pointer_type) {
                                flags.value.pointer = va_arg(list, void *);

                        } else if(flags.no_value == 0) {
                                if(flags.is_unsigned) {
                                        switch(flags.length) {
                                        case LenChar:
                                                flags.value.vunsigned = (unsigned char) va_arg(list, unsigned int);

                                                break;

                                        case LenShort:
                                                flags.value.vunsigned = (unsigned short) va_arg(list, unsigned int);

                                                break;

                                        case LenInt:
                                                flags.value.vunsigned = va_arg(list, unsigned int);

                                                break;

                                        case LenLong:
                                                flags.value.vunsigned = va_arg(list, unsigned long);

                                                break;

#ifdef PRINTF_LONGLONG
                                        case LenLongLong:
                                                flags.value.vunsigned = va_arg(list, unsigned long long);

                                                break;
#endif
                                        default:
                                                break;
                                        }
                                } else {
                                        switch(flags.length) {
                                        case LenChar:
                                                flags.value.vsigned = (char) va_arg(list, int);

                                                break;

                                        case LenShort:
                                                flags.value.vsigned = (short) va_arg(list, int);

                                                break;

                                        case LenInt:
                                                flags.value.vsigned = va_arg(list, int);

                                                break;

                                        case LenLong:
                                                flags.value.vsigned = va_arg(list, long);

                                                break;

#ifdef PRINTF_LONGLONG
                                        case LenLongLong:
                                                flags.value.vsigned = va_arg(list, long long);

                                                break;
#endif
                                        default:
                                                break;
                                        }
                                }

                                switch(flags.length) {
                                case LenSize:
                                        flags.value.vunsigned = va_arg(list, size_t);

                                        break;

                                case LenPtrdiff:
                                        flags.value.vsigned = va_arg(list, ptrdiff_t);

                                        break;

                                default:
                                        break;
                                }
                        }

                        // Conversion

                        switch(*fmt) {
                        case '%':
                                len++;

                                callback('%', pass);

                                break;

                        case 'u':
                        case 'i':
                        case 'd':
                                len += outnum(&flags, 10, callback, pass);

                                break;

                        case 'o':
                                len += outnum(&flags, 8, callback, pass);

                                break;

                        case 'p':
                                flags.precision = 2 * __SIZEOF_POINTER__;
                                flags.alternate = 1;

                        case 'X':
                                flags.upper = 1;

                        case 'x':
                                len += outnum(&flags, 16, callback, pass);

                                break;
                        case 'c':
                                len++;

                                callback((char) flags.value.vsigned, pass);

                                break;

                         case 's':
                                len += outs(&flags, callback, pass);

                                break;

                        case 'n':
                                place_len(&flags, len);

                                break;
                        }

                } else {
                        len++;
                        callback(*fmt, pass);
                }
        }

        return len;
}

