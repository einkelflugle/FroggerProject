/*
 * joystick.c
 *
 * Author: Max Miller
 */

#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "game.h"

#define JOYSTICK_IDLE 0
#define JOYSTICK_UP 1
#define JOYSTICK_UP_RIGHT 2
#define JOYSTICK_RIGHT 3
#define JOYSTICK_DOWN_RIGHT 4
#define JOYSTICK_DOWN 5
#define JOYSTICK_DOWN_LEFT 6
#define JOYSTICK_LEFT 7
#define JOYSTICK_UP_LEFT 8

void init_joystick(void) {
	/* Set up ADC - AVCC reference, right adjust */
	ADMUX = (1<<REFS0);
	
	/* Turn on the ADC (but don't start a conversion yet).
	 * Choose a clock divider of 64.
	 */
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1);
}

/* Returns true if the frog has moved given the joystick direction. */
uint8_t joystick_move(uint8_t joystick_direction) {
	switch (joystick_direction) {
		case JOYSTICK_IDLE:
			return 0;
		case JOYSTICK_UP:
			move_frog_forward();
			return 1;
		case JOYSTICK_RIGHT:
			move_frog_to_right();
			return 1;
		case JOYSTICK_DOWN:
			move_frog_backward();
			return 1;
		case JOYSTICK_LEFT:
			move_frog_to_left();
			return 1;
		case JOYSTICK_UP_RIGHT:
			move_frog_up_right();
			return 1;
		case JOYSTICK_DOWN_RIGHT:
			move_frog_down_right();
			return 1;
		case JOYSTICK_UP_LEFT:
			move_frog_up_left();
			return 1;
		case JOYSTICK_DOWN_LEFT:
			move_frog_down_left();
			return 1;
		default:
			return 0;
	}
}

uint8_t poll_joystick_direction(void) {
	uint16_t x, y; // Current position of the joystick in each axis
	
	// Set the ADC mux to ADC0 (left/right direction)
	ADMUX &= ~1;
	// Start the ADC conversion
	ADCSRA |= (1<<ADSC);
	
	while (ADCSRA & (1<<ADSC)) {
		; // Wait until conversion is finished
	}
	// Read the value
	x = ADC;
	
	// Set the ADC mux to ADC1 (up/down direction)
	ADMUX |= 1;
	// Start the ADC conversion
	ADCSRA |= (1<<ADSC);
	
	while (ADCSRA & (1<<ADSC)) {
		; // Wait until conversion is finished
	}
	// Read the value
	y = ADC;
	
	// Convert the x and y values to a joystick direction defined
	// at the top of this file
	uint8_t direction;
	if (x > 700) {
		if (y > 700) {
			direction = JOYSTICK_UP_RIGHT;
		} else if (y < 400) {
			direction = JOYSTICK_DOWN_RIGHT;
		} else {
			direction = JOYSTICK_RIGHT;
		}
	} else if (x < 300) {
		if (y > 700) {
			direction = JOYSTICK_UP_LEFT;
		} else if (y < 400) {
			direction = JOYSTICK_DOWN_LEFT;
		} else {
			direction = JOYSTICK_LEFT;
		}
	} else {
		if (y > 900) {
			direction = JOYSTICK_UP;
		} else if (y < 150) {
			direction = JOYSTICK_DOWN;
		} else {
			direction = JOYSTICK_IDLE;
		}
	}
	return direction;
}