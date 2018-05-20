/*
 * sevenseg.c
 *
 * A seven segment display is connected to port C pins 0-7, with the CC
 * (digit select) pin connected to port A, pin 3.
 *
 *  Author: maxcmiller
 */ 

#include "timer0.h"

#include <stdio.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

// Seven segment display - segment values for digits 0 to 9 and 0 with a decimal point
static uint8_t seven_seg[11] = { 63,6,91,79,102,109,125,7,127,111,191};
// Current digit being displayed: 0 = right (ones place), 1 = left (tens place)
static uint8_t current_digit = 0;
	
/* Display digit function. Arguments are the digit number (0 to 9)
 * and the digit to display it on (0 = right, 1 = left). The function 
 * outputs the correct seven segment display value to port C and the 
 * correct digit select value to port A, pin 3.
 */
static void display_digit(uint8_t number, uint8_t digit) 
{
	// Set port A pin 3 to the digit (1 or 0)
	if (digit == 1) {
		PORTA |= (1 << 3);
	} else {
		PORTA &= ~(1 << 3);
	}
	PORTC = seven_seg[number];	// We assume digit is in range 0 to 9
}

/* Displays the given time_remaining value (in ms) to the seven segment display.
 * Called repeatedly while the game is being played.
 */
void display_ssd(uint32_t time_remaining) {
	uint8_t value;
	// Two cases: time remaining < 1 second, time remaining >= 1 second
	if (time_remaining < 1000) {
		// Draw tenths of a second with a decimal point
		if (current_digit == 0) {
			value = (time_remaining / 100) % 10;
		} else {
			value = 10; // 0 with decimal point
		}
	} else {
		if (current_digit == 0) {
			/* Extract the ones place of the number of seconds remaining */
			value = (time_remaining / 1000) % 10;
		} else {
			/* Extract the tens place of the number of seconds remaining */
			value = (time_remaining / 10000) % 10;
			if (value == 0) {
				// Don't draw a 0 on the left digit
				current_digit = 1 - current_digit;
				return;
			}
		}
	}
	
	display_digit(value, current_digit);
	/* Change the digit flag for next time. 0 becomes 1, 1 becomes 0. */
	current_digit = 1 - current_digit;
}

void init_sevenseg(void) {
	/* Set port C (all pins) to be outputs */
	DDRC = 0xFF;
	
	/* Set port A, pin 3 to be an output */
	DDRA |= (1 << 3);
}