#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "text.h"

/* The shared text builders (text.c). */
int main(void)
{
    char buf[32];
    char *end;

    end = text_append(buf, "OUT BY ");
    assert(end == buf + 7 && strcmp(buf, "OUT BY ") == 0);
    end = text_count(end, 12);
    end = text_append(end, " INCHES!");
    assert(strcmp(buf, "OUT BY 12 INCHES!") == 0 && *end == 0);
    assert(text_append(buf, "") == buf && buf[0] == 0);

    text_count(buf, 0);
    assert(strcmp(buf, "0") == 0);
    text_count(buf, 7);
    assert(strcmp(buf, "7") == 0);
    text_count(buf, 10);
    assert(strcmp(buf, "10") == 0);
    text_count(buf, 100);
    assert(strcmp(buf, "100") == 0);
    text_count(buf, 9999);
    assert(strcmp(buf, "9999") == 0);
    end = text_count(buf, 65535);
    assert(strcmp(buf, "65535") == 0 && end == buf + 5);
    text_count(buf, 10203);
    assert(strcmp(buf, "10203") == 0);

    end = text_count4(buf, 0);
    assert(strcmp(buf, "   0") == 0 && end == buf + 4);
    text_count4(buf, 5);
    assert(strcmp(buf, "   5") == 0);
    text_count4(buf, 40);
    assert(strcmp(buf, "  40") == 0);
    text_count4(buf, 1005);
    assert(strcmp(buf, "1005") == 0);
    text_count4(buf, 9999);
    assert(strcmp(buf, "9999") == 0);
    printf("text host checks passed\n");
    return 0;
}
