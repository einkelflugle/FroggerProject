/*
 * highscores.c
 *
 * Author: Max Miller
 */ 

#include <stdio.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "terminalio.h"

/* The letter J (ASCII 74) is the required signature,
 * denoting that a particular block of EEPROM is storing
 * a score.
 */
#define SIGNATURE 74

uint8_t EEMEM main_signature;
uint8_t EEMEM signature_one;
uint16_t EEMEM score_one;
uint8_t EEMEM name_one[10];
uint8_t EEMEM signature_two;
uint16_t EEMEM score_two;
uint8_t EEMEM name_two[10];
uint8_t EEMEM signature_three;
uint16_t EEMEM score_three;
uint8_t EEMEM name_three[10];
uint8_t EEMEM signature_four;
uint16_t EEMEM score_four;
uint8_t EEMEM name_four[10];
uint8_t EEMEM signature_five;
uint16_t EEMEM score_five;
uint8_t EEMEM name_five[10];

uint16_t scores[5];
uint8_t names[5][10];
uint8_t slots_used;

uint8_t is_high_score(uint16_t score) {
	// Is there a score higher than the one passed?
	uint8_t higher_score_found = 0; // bool
	for (int i = 0; i < 5; i++) {
		if (scores[i] > score) {
			higher_score_found = 1;
		}
	}
	return !higher_score_found;
}

void save_high_score(uint16_t score, uint8_t *name) {
	// Is there still a free slot
	if (slots_used < 5) {
		// Yes there is, find it
		if (eeprom_read_byte(&signature_one) != SIGNATURE) {
			// Slot one is free, write to it
			eeprom_write_byte(&signature_one, SIGNATURE);
			eeprom_write_word(&score_one, score);
			eeprom_write_block(name, &name_one, 10);
		}
		else if (eeprom_read_byte(&signature_two) != SIGNATURE) {
			eeprom_write_byte(&signature_two, SIGNATURE);
			eeprom_write_word(&score_two, score);
			eeprom_write_block(name, &name_two, 10);
		}
		else if (eeprom_read_byte(&signature_three) != SIGNATURE) {
			eeprom_write_byte(&signature_three, SIGNATURE);
			eeprom_write_word(&score_three, score);
			eeprom_write_block(name, &name_three, 10);
		}
		else if (eeprom_read_byte(&signature_four) != SIGNATURE) {
			eeprom_write_byte(&signature_four, SIGNATURE);
			eeprom_write_word(&score_four, score);
			eeprom_write_block(name, &name_four, 10);
		}
		else if (eeprom_read_byte(&signature_five) != SIGNATURE) {
			eeprom_write_byte(&signature_five, SIGNATURE);
			eeprom_write_word(&score_five, score);
			eeprom_write_block(name, &name_five, 10);
		}
	}
}

void draw_high_scores(void) {
	move_cursor(33, 6);
	set_display_attribute(TERM_BRIGHT);
	printf_P(PSTR("%s"), "High Scores");
	set_display_attribute(TERM_RESET);
	
	if (eeprom_read_byte(&main_signature) != SIGNATURE) {
		move_cursor(33, 8);
		printf_P(PSTR("%s"), "No high scores yet.");
		// Initialise high scores by writing signature value to EEPROM
		eeprom_write_byte(&main_signature, 'J');
		return;
	}
	
	// Check for signature
	if (eeprom_read_byte(&signature_one) == SIGNATURE) {
		scores[0] = eeprom_read_word(&score_one);
		eeprom_read_block((void*) &names[0], (const void*) &name_one, 10);
		slots_used++;
	}
	if (eeprom_read_byte(&signature_two) == SIGNATURE) {
		scores[1] = eeprom_read_word(&score_two);
		eeprom_read_block((void*) &names[1], (const void*) &name_two, 10);
		slots_used++;
	}
	if (eeprom_read_byte(&signature_three) == SIGNATURE) {
		scores[2] = eeprom_read_word(&score_three);
		eeprom_read_block((void*) &names[2], (const void*) &name_three, 10);
		slots_used++;
	}
	if (eeprom_read_byte(&signature_four) == SIGNATURE) {
		scores[3] = eeprom_read_word(&score_four);
		eeprom_read_block((void*) &names[3], (const void*) &name_four, 10);
		slots_used++;
	}
	if (eeprom_read_byte(&signature_one) == SIGNATURE) {
		scores[4] = eeprom_read_word(&score_five);
		eeprom_read_block((void*) &names[4], (const void*) &name_five, 10);
		slots_used++;
	}
	
	// Sort the array of found scores into descending order
	for (int i = 0; i < 5; i++) {
		for (int j = i + 1; j < 5; j++) {
			if (scores[i] < scores[j]) {
				uint8_t temp_score;
				uint8_t temp_name[10];
				
				temp_score = scores[i];
				// Set temp_name to names[i]
				for (int k = 0; k < 10; k++) {
					temp_name[k] = names[i][k];
				}
				
				scores[i] = scores[j];
				scores[j] = temp_score;
				
				// set names[i] to names[j]
				for (int k = 0; k < 10; k++) {
					names[i][k] = names[j][k];
				}
				// set names[j] to temp_name
				for (int k = 0; k < 10; k++) {
					names[j][k] = temp_name[k];
				}
			}
		}
	}

	// Display the names and scores to the high scores area
	for (int i = 0; i < slots_used; i++) {
		move_cursor(33, 8 + i);
		printf_P(PSTR("%d. %d - %s"), i + 1, scores[i], names[i]);
	}
}