/*
 * FroggerProject.c
 *
 * Main file
 *
 * Author: Peter Sutton. Modified by <YOUR NAME HERE>
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>

#include "ledmatrix.h"
#include "scrolling_char_display.h"
#include "buttons.h"
#include "serialio.h"
#include "terminalio.h"
#include "score.h"
#include "sevenseg.h"
#include "timer0.h"
#include "game.h"

#define F_CPU 8000000L
#include <util/delay.h>

// Function prototypes - these are defined below (after main()) in the order
// given here
void initialise_hardware(void);
void splash_screen(void);
void new_game(void);
void play_game(void);
void handle_game_over(void);

// ASCII code for Escape character
#define ESCAPE_CHAR 27

/////////////////////////////// main //////////////////////////////////
int main(void) {
	// Setup hardware and call backs. This will turn on 
	// interrupts.
	initialise_hardware();
	
	// Show the splash screen message. Returns when display
	// is complete
	splash_screen();
	
	while(1) {
		new_game();
		play_game();
		handle_game_over();
	}
}

void initialise_hardware(void) {
	ledmatrix_setup();
	init_button_interrupts();
	// Setup serial port for 19200 baud communication with no echo
	// of incoming characters
	init_serial_stdio(19200,0);
	
	init_timer0();
	
	// Initialise the LEDs used for displaying the number of lives remaining
	// LEDs are on the upper 4 bits of Port A
	DDRA |= 0xF0;
	
	// Initialise the Seven Segment Display for showing the time left this life
	init_sevenseg();
	
	// Turn on global interrupts
	sei();
}

void splash_screen(void) {
	// Clear terminal screen and output a message
	clear_terminal();
	move_cursor(10,10);
	printf_P(PSTR("Frogger"));
	move_cursor(10,12);
	printf_P(PSTR("CSSE2010/7201 project by Max Miller (s44080118)"));
	
	// Output the scrolling message to the LED matrix
	// and wait for a push button to be pushed.
	ledmatrix_clear();
	while(1) {
		set_scrolling_display_text("FROGGER    S4480118", COLOUR_GREEN);
		// Scroll the message until it has scrolled off the 
		// display or a button is pushed
		while(scroll_display()) {
			_delay_ms(150);
			if(button_pushed() != NO_BUTTON_PUSHED) {
				return;
			}
		}
	}
}

void new_game(void) {
	// Clear the serial terminal
	clear_terminal();
	
	// Initialise the score
	init_score();
	
	// Set number of lives to maximum value
	init_lives();
	
	// Set the level to initial value
	init_level();
	
	// Initialise the game and display
	initialise_game();
	
	// Clear a button push or serial input if any are waiting
	// (The cast to void means the return value is ignored.)
	(void)button_pushed();
	clear_serial_input_buffer();
}

void play_game(void) {
	uint32_t current_time = get_current_time();
	// Time limit for each frog in ms
	uint32_t time_limit = 18000;
	// Time at which the current frog began its life
	uint32_t begin_life_time = current_time;
	uint32_t last_move_times[5]; // 5 unique scrolling speeds
	// Each speed has a time between scrolls in milliseconds
	uint32_t scroll_times[5] = {1000, 1150, 750, 1300, 900};
	int8_t button;
	char serial_input, escape_sequence_char;
	uint8_t characters_into_escape_sequence = 0;
	uint8_t is_paused = 0;
	uint32_t time_pause_began = 0;
	
	// Remember the current time as the last time the vehicles and logs were moved.
	for (int i = 0; i < 5; i++) {
		last_move_times[i] = current_time;
	}
	
	// We play the game while we have lives left
	while(get_lives_remaining()) {
		if(!is_frog_dead() && frog_has_reached_riverbank()) {
			// Frog reached the other side successfully but the
			// riverbank isn't full, put a new frog at the start
			put_frog_in_start_position();
			// Add 10 to score, frog reached other side successfully
			add_to_score(10);
			// Reset begin_life_time to current time
			begin_life_time = get_current_time();
		}
		
		// We have completed a level, progress to the next one
		if (is_riverbank_full()) {
			// Shift the LED matrix left
			for (int i = 0; i < MATRIX_NUM_COLUMNS; i++) {
				ledmatrix_shift_display_left();
				_delay_ms(70);
			}
			// Increment the level number
			set_level(get_level() + 1);
			// Restore a life (function handles checking if greater than max)
			set_lives(get_lives_remaining() + 1);
			// Reset the game state
			initialise_game();
			// Reset begin_life_time to current time
			begin_life_time = get_current_time();
		}
		
		// Is the frog dead or the time limit has been reached while unpaused
		if (is_frog_dead() || (get_current_time() > begin_life_time + time_limit && !is_paused)) {
			// Can the player continue playing?
			if (get_lives_remaining() > 1) {
				move_cursor(10,14);
				printf_P(PSTR("YOU DIED"));
				move_cursor(10,15);
				printf_P(PSTR("Press a button to spend a life"));
				// Wait until player acknowledges
				while(button_pushed() == NO_BUTTON_PUSHED) {
					; // wait
				}
				(void) button_pushed();
				clear_serial_input_buffer();
				
				// Clear both lines
				move_cursor(10,14);
				clear_to_end_of_line();
				move_cursor(10,15);
				clear_to_end_of_line();
			}
			
			// Decrement the number of lives
			set_lives(get_lives_remaining() - 1);
			
			// Frog is dead, put new frog in start position
			put_frog_in_start_position();
			
			// Reset begin_life_time
			begin_life_time = get_current_time();
		}
		
		// Check for input - which could be a button push or serial input.
		// Serial input may be part of an escape sequence, e.g. ESC [ D
		// is a left cursor key press. At most one of the following three
		// variables will be set to a value other than -1 if input is available.
		// (We don't initalise button to -1 since button_pushed() will return -1
		// if no button pushes are waiting to be returned.)
		// Button pushes take priority over serial input. If there are both then
		// we'll retrieve the serial input the next time through this loop
		serial_input = -1;
		escape_sequence_char = -1;
		button = button_pushed();
		
		if(button == NO_BUTTON_PUSHED) {
			// No push button was pushed, see if there is any serial input
			if(serial_input_available()) {
				// Serial data was available - read the data from standard input
				serial_input = fgetc(stdin);
				// Check if the character is part of an escape sequence
				if(characters_into_escape_sequence == 0 && serial_input == ESCAPE_CHAR) {
					// We've hit the first character in an escape sequence (escape)
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
				} else if(characters_into_escape_sequence == 1 && serial_input == '[') {
					// We've hit the second character in an escape sequence
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
				} else if(characters_into_escape_sequence == 2) {
					// Third (and last) character in the escape sequence
					escape_sequence_char = serial_input;
					serial_input = -1;  // Don't further process this character - we
										// deal with it as part of the escape sequence
					characters_into_escape_sequence = 0;
				} else {
					// Character was not part of an escape sequence (or we received
					// an invalid second character in the sequence). We'll process 
					// the data in the serial_input variable.
					characters_into_escape_sequence = 0;
				}
			}
		}
		
		// Process the input. 
		if(button==3 || escape_sequence_char=='D' || serial_input=='L' || serial_input=='l') {
			// Attempt to move left
			// Only attempt to move if the game isn't paused
			if (!is_paused) move_frog_to_left();
		} else if(button==2 || escape_sequence_char=='A' || serial_input=='U' || serial_input=='u') {
			// Attempt to move forward
			if (!is_paused) move_frog_forward();
		} else if(button==1 || escape_sequence_char=='B' || serial_input=='D' || serial_input=='d') {
			// Attempt to move down
			if (!is_paused) move_frog_backward();
		} else if(button==0 || escape_sequence_char=='C' || serial_input=='R' || serial_input=='r') {
			// Attempt to move right
			if (!is_paused) move_frog_to_right();
		} else if(serial_input == 'p' || serial_input == 'P') {
			// If we are pausing, store the current time to add to begin_life_time after unpausing
			if (!is_paused) {
				time_pause_began = get_current_time();
			} else {
				// If we are unpausing, add the time paused to begin_life_time
				begin_life_time += (get_current_time() - time_pause_began);
			}
			// Pause/unpause the game until 'p' or 'P' is
			// pressed again
			is_paused = ~is_paused;
		} 
		// else - invalid input or we're part way through an escape sequence -
		// do nothing
		
		// Level speed multiplier = x/4 + 3/4 where x is the current level
		double level_speed_multiplier = (get_level() / 4.0) + (3.0/4.0);
		current_time = get_current_time();
		if(!is_frog_dead()) { 
			// Only check for scroll times if the frog is still alive
			if (current_time >= last_move_times[0] + scroll_times[0] / level_speed_multiplier) {
				// scroll_times[x] milliseconds have passed since the last time we moved
				// this row - move it again and keep track of the time when we did this. 
				// Only scroll if the game isn't paused
				if (!is_paused) scroll_vehicle_lane(0, 1);
				last_move_times[0] = current_time;
			}
			if (current_time >= last_move_times[1] + scroll_times[1] / level_speed_multiplier) {
				if (!is_paused) scroll_vehicle_lane(1, -1);
				last_move_times[1] = current_time;
			}
			if (current_time >= last_move_times[2] + scroll_times[2] / level_speed_multiplier) {
				if (!is_paused) scroll_vehicle_lane(2, 1);
				last_move_times[2] = current_time;
			}
			if (current_time >= last_move_times[3] + scroll_times[3] / level_speed_multiplier) {
				if (!is_paused) scroll_river_channel(0, -1);
				last_move_times[3] = current_time;
			}
			if (current_time >= last_move_times[4] + scroll_times[4] / level_speed_multiplier) {
				if (!is_paused) scroll_river_channel(1, 1);
				last_move_times[4] = current_time;
			}
		}
		
		// Update the seven segment display with the time remaining this life
		uint32_t time_remaining;
		if (is_paused) {
			// Time remaining = [time limit] - [time spent up to pause started]
			time_remaining = time_limit - (time_pause_began - begin_life_time);
		} else {
			// Time remaining = [time limit] - [time spent so far]
			time_remaining = time_limit - (get_current_time() - begin_life_time);
		}
		display_ssd(time_remaining);
	}
	// We get here if we have run out of lives
	// The game is over.
	
	// Game has been completed successfully, add 10 to score for last frog
	// reaching the other side.
	if (is_riverbank_full() && !is_frog_dead()) {
		add_to_score(10);
	}
}

void handle_game_over() {
	move_cursor(10,14);
	printf_P(PSTR("GAME OVER"));
	move_cursor(10,15);
	printf_P(PSTR("Press a button to start again"));
	while(button_pushed() == NO_BUTTON_PUSHED) {
		; // wait
	}
	
}