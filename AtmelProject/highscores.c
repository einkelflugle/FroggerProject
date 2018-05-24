/*
 * highscores.c
 *
 * Author: Max Miller
 */ 

#include <stdio.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "terminalio.h"

uint8_t EEMEM main_signature;

static uint8_t read_main_signature(void) {
	uint8_t sig;
	
	sig = eeprom_read_byte(&main_signature);
	return sig;
}

void draw_high_scores(void) {
	move_cursor(33, 6);
	set_display_attribute(TERM_BRIGHT);
	printf_P(PSTR("%s"), PSTR("High Scores"));
	
	uint8_t str[100];
	eeprom_read_block((void*) &str, (const void*) 0, 100);
	move_cursor(33, 8);
	printf_P(PSTR("%s"), str);
	
	uint8_t name[10];
	printf_P(PSTR("Enter name: "));
	scanf_P(PSTR("%s"), name);
	printf_P(PSTR("%s"), name);
}