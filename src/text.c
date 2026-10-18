#include "text.h"

char *text_append(char *out, const char *text)
{
    while (*text)
        *out++ = *text++;
    *out = 0;
    return out;
}

char *text_count(char *out, u16 value)
{
    static const u16 powers[4] = { 10000, 1000, 100, 10 };
    char digit;
    /* Start at the leading digit: most counts are one or two digits, and
     * each skipped power is a loop pass for 816-tcc (R33). */
    u8 i = value < 10 ? 4 : value < 100 ? 3 : value < 1000 ? 2 :
        value < 10000 ? 1 : 0;

    for (; i < 4; ++i) {
        digit = '0';
        while (value >= powers[i]) {
            value -= powers[i];
            ++digit;
        }
        *out++ = digit;
    }
    *out++ = (char)('0' + value);
    *out = 0;
    return out;
}

char *text_count4(char *out, u16 value)
{
    static const u16 powers[3] = { 1000, 100, 10 };
    u8 i;
    u8 leading = 1;
    char digit;

    /* The digit in a local: each out[i] is a pointer access for 816-tcc. */
    for (i = 0; i < 3; ++i) {
        digit = '0';
        while (value >= powers[i]) {
            value -= powers[i];
            ++digit;
        }
        if (leading && digit == '0')
            digit = ' ';
        else
            leading = 0;
        *out++ = digit;
    }
    *out++ = (char)('0' + value);
    *out = 0;
    return out;
}
