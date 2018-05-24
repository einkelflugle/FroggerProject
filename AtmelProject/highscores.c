/*
 * highscores.c
 *
 * Author: Max Miller
 */ 

#include <stdio.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "terminalio.h"

uint8_t EEMEM signature;
uint16_t EEMEM score_one;
uint8_t EEMEM name_one[10];
uint16_t EEMEM score_two;
uint8_t EEMEM name_two[10];
uint16_t EEMEM score_three;
uint8_t EEMEM name_three[10];
uint16_t EEMEM score_four;
uint8_t EEMEM name_four[10];
uint16_t EEMEM score_five;
uint8_t EEMEM name_five[10];

static uint8_t read_signature(void) {
	uint8_t sig;
	
	sig = eeprom_read_byte(&signature);
	return sig;
}

void draw_high_scores(void) {
	move_cursor(33, 6);
	set_display_attribute(TERM_BRIGHT);
	printf_P(PSTR("%s"), "High Scores");
	set_display_attribute(TERM_RESET);
	
	if (read_signature() != 52) {
		// number 4 (ASCII 52) is the required signature
		move_cursor(33, 8);
		printf_P(PSTR("%s %c"), "No high scores yet.", read_signature());
		return;
	}
	
	uint16_t score;
	uint8_t name[10];
	
	score = eeprom_read_word(&score_one);
	eeprom_read_block((void*) &name, (const void*) &name_one, 10);
	move_cursor(33, 8);
	printf_P(PSTR("1. %d - %s"), score, name);
	
	score = eeprom_read_word(&score_two);
	eeprom_read_block((void*) &name, (const void*) &name_two, 10);
	move_cursor(33, 9);
	printf_P(PSTR("2. %d - %s"), score, name);
	
	score = eeprom_read_word(&score_three);
	eeprom_read_block((void*) &name, (const void*) &name_three, 10);
	move_cursor(33, 10);
	printf_P(PSTR("3. %d - %s"), score, name);
	
	score = eeprom_read_word(&score_four);
	eeprom_read_block((void*) &name, (const void*) &name_four, 10);
	move_cursor(33, 11);
	printf_P(PSTR("4. %d - %s"), score, name);
	
	score = eeprom_read_word(&score_five);
	eeprom_read_block((void*) &name, (const void*) &name_five, 10);
	move_cursor(33, 12);
	printf_P(PSTR("5. %d - %s"), score, name);
}