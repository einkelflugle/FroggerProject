/*
 * score.c
 *
 * Written by Peter Sutton
 */

#include "score.h"
#include "terminalio.h"
#include <stdio.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

uint16_t score;

void init_score(void) {
	score = 0;
	// Display the score on the terminal
	move_cursor(1,1);
	printf_P(PSTR("Score: %4d"), score);
}

void add_to_score(uint16_t value) {
	score += value;
	move_cursor(1,1);
	printf_P(PSTR("Score: %4d"), score);
}

uint16_t get_score(void) {
	return score;
}
