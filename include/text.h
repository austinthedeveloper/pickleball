#ifndef TEXT_H
#define TEXT_H

#include "game_types.h"

/*
 * Building HUD text in a buffer: appends and decimal counts, without
 * 816-tcc's division calls (repeated subtraction). Each writes a
 * terminator and returns where it is, so calls chain. Pure.
 */

char *text_append(char *out, const char *text);
/* value with no leading zeros: "0", "7", "12", "65535". */
char *text_count(char *out, u16 value);
/* value right-aligned in four cells, leading zeros as spaces ("  12");
 * up to 9999 (the records saturate there, SAVE_COUNTER_MAX). */
char *text_count4(char *out, u16 value);

#endif
