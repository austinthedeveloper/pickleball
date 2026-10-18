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
    u8 i;
    u8 leading = 1;
    char digit;

    for (i = 0; i < 4; ++i) {
        digit = '0';
        while (value >= powers[i]) {
            value -= powers[i];
            ++digit;
        }
        if (!leading || digit != '0') {
            *out++ = digit;
            leading = 0;
        }
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

    for (i = 0; i < 3; ++i) {
        out[i] = '0';
        while (value >= powers[i]) {
            value -= powers[i];
            ++out[i];
        }
        if (leading && out[i] == '0')
            out[i] = ' ';
        else
            leading = 0;
    }
    out[3] = (char)('0' + value);
    out[4] = 0;
    return out + 4;
}
