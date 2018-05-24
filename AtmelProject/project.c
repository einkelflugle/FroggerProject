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
#include "highscores.h"
#include "serialio.h"
#include "terminalio.h"
#include "score.h"
#include "sevenseg.h"
#include "sound_effects.h"
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
static void draw_splash_frog();

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
	
	// Initialise sound effects generated via timer/counter 1 and the piezo buzzer
	init_sound_effects();
	
	// Turn on global interrupts
	sei();
}

void splash_screen(void) {
	// Clear terminal screen and output a message
	clear_terminal();
	move_cursor(10,2);
	set_display_attribute(TERM_BRIGHT);
	printf_P(PSTR("Frogger"));
	move_cursor(10,4);
	set_display_attribute(TERM_RESET);
	printf_P(PSTR("CSSE2010/7201 project by Max Miller (s44080118)"));
	
	draw_splash_frog();
	draw_high_scores();
	
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
	uint32_t last_button_pushed_at = 0; // time in ms
	// Time between simulated button presses when holding down a button
	uint16_t button_hold_delay = 200;
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
			// riverbank isn't full
			// Play a sound
			play_sound_reached_riverbank();
			// Put a new frog at the start
			put_frog_in_start_position();
			// Add 10 to score, frog reached other side successfully
			add_to_score(10);
			// Reset begin_life_time to current time
			begin_life_time = get_current_time();
		}
		
		// We have completed a level, progress to the next one
		if (is_riverbank_full()) {
			// Play the new level sound
			play_sound_new_level();
			// Shift the LED matrix left
			for (int i = 0; i < MATRIX_NUM_COLUMNS; i++) {
				ledmatrix_shift_display_left();
				update_sound_effects();
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
				play_sound_death();
				for (int i = 0; i < 10; i++) {
					update_sound_effects();
					_delay_ms(100); // Wait for 1000ms
				}
				stop_sound();
				(void) button_pushed();
				clear_serial_input_buffer();
				
				// Clear both lines
				move_cursor(10,14);
				clear_to_end_of_line();
				move_cursor(10,15);
				clear_to_end_of_line();
				
				// Frog is dead, put new frog in start position
				put_frog_in_start_position();
				
				// Reset begin_life_time
				begin_life_time = get_current_time();
			}
			
			// Decrement the number of lives
			set_lives(get_lives_remaining() - 1);
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
		} else {
			// A button was pushed
			last_button_pushed_at = get_current_time();
		}
		
		// Process the input. 
		if(button==3 || escape_sequence_char=='D' || serial_input=='L' || serial_input=='l') {
			// Attempt to move left
			// Only attempt to move if the game isn't paused
			if (!is_paused) move_frog_to_left();
			play_sound_frog_move();
		} else if(button==2 || escape_sequence_char=='A' || serial_input=='U' || serial_input=='u') {
			// Attempt to move forward
			if (!is_paused) move_frog_forward();
			play_sound_frog_move();
		} else if(button==1 || escape_sequence_char=='B' || serial_input=='D' || serial_input=='d') {
			// Attempt to move down
			if (!is_paused) move_frog_backward();
			play_sound_frog_move();
		} else if(button==0 || escape_sequence_char=='C' || serial_input=='R' || serial_input=='r') {
			// Attempt to move right
			if (!is_paused) move_frog_to_right();
			play_sound_frog_move();
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
		
		// Find whether a button is currently being held down
		int8_t button_held_down;
		if ((PINB & 0x0F) == 0b00000001) {
			// B0 is being pushed
			button_held_down = 0;
		} else if ((PINB & 0x0F) == 0b00000010) {
			// B1 is being pushed
			button_held_down = 1;
		} else if ((PINB & 0x0F) == 0b00000100) {
			// B2 is being pushed
			button_held_down = 2;
		} else if ((PINB & 0x0F) == 0b00001000) {
			// B3 is being pushed
			button_held_down = 3;
		} else {
			// No button is being pushed
			button_held_down = -2;
		}
		
		current_time = get_current_time();
		if (last_button_pushed_at && button_held_down != -2) {
			// Avoids registering movement just after game begins
			if (current_time > last_button_pushed_at + button_hold_delay) {
				if (button_held_down == 3) {
					if (!is_paused) move_frog_to_left();
					play_sound_frog_move();
					} else if (button_held_down == 2) {
					if (!is_paused) move_frog_forward();
					play_sound_frog_move();
					} else if (button_held_down == 1) {
					if (!is_paused) move_frog_backward();
					play_sound_frog_move();
					} else if (button_held_down == 0) {
					if (!is_paused) move_frog_to_right();
					play_sound_frog_move();
				}
				last_button_pushed_at = current_time;
			}
		}
		
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
		
		// Update the sound effects queue
		update_sound_effects();
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
	move_cursor(13,2);
	set_display_attribute(TERM_BRIGHT);
	printf_P(PSTR("Game Over"));
	set_display_attribute(TERM_RESET);
	
	play_sound_game_over();
	while (is_playing_sound()) {
		update_sound_effects();
	}
	
	// If a top 5 score was achieved, prompt the user for their name
	if (get_score() > 1) {
		uint8_t name[10] = {0};
		uint8_t chars = 0;
		uint8_t return_pressed = 0; // bool
		
		move_cursor(13, 3);
		printf_P(PSTR("Enter name: "));
		while (!return_pressed) {
			uint8_t c;
			scanf_P(PSTR("%c"), &c);
			// Check for return (enter)
			if (c == '\n') {
				return_pressed = 1;
				continue;
			}
			// Check for backspace (ASCII for delete/backspace)
			if ((c == 8 || c == 127) && chars > 0) {
				// Clear all input then redraw it with last char removed
				move_cursor(13 + 12, 3);
				clear_to_end_of_line();
				move_cursor(13 + 12, 3);
				name[chars - 1] = 0;
				chars--;
				printf_P(PSTR("%s"), name);
			}
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == ' ') {
				if (chars == 10) {
					continue;
				}
				printf_P(PSTR("%c"), c);
				name[chars] = c;
				chars++;
			}
		}
		// Save name and score to EEPROM
		for (int i = 0; i < 10; i++) {
			printf_P(PSTR("\n`%c`"), name[i]);
		}
	}
	
	// Cancel any button pressed or serial input
	(void) button_pushed();
	clear_serial_input_buffer();
	
	move_cursor(13,4);
	printf_P(PSTR("Press a button to start again"));
	while(button_pushed() == NO_BUTTON_PUSHED) {
		; // wait
	}
	
}

static void draw_splash_frog(void) {
	uint8_t canvas_x = 10;
	uint8_t canvas_y = 6;
	uint8_t x = 12; // Top left corner x
	uint8_t y = 7; // Top right corner y
	// Draw bounding canvas
	for (int i = 0; i < 21; i++) {
		set_display_attribute(FG_CYAN);
		draw_vertical_line(canvas_x + i, canvas_y, 23);
	}
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y, x + 3, x + 5);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y, x + 11, x + 13);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 1, x + 2, x + 2);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 1, x + 3, x + 5);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 1, x + 6, x + 6);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 1, x + 10, x + 10);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 1, x + 11, x + 13);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 1, x + 14, x + 14);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 2, x + 1, x + 1);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 2, x + 2, x + 2);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 2, x + 3, x + 4);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 2, x + 5, x + 6);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 2, x + 7, x + 9);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 2, x + 10, x + 11);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 2, x + 12, x + 13);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 2, x + 14, x + 14);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 2, x + 15, x + 15);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 3, x + 1, x + 1);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 3, x + 2, x + 2);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 3, x + 3, x + 4);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 3, x + 5, x + 6);
	set_display_attribute(FG_GREEN);
	draw_horizontal_line(y + 3, x + 7, x + 9);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 3, x + 10, x + 11);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 3, x + 12, x + 13);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 3, x + 14, x + 14);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 3, x + 15, x + 15);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 4, x + 1, x + 1);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 4, x + 2, x + 6);
	set_display_attribute(FG_GREEN);
	draw_horizontal_line(y + 4, x + 7, x + 9);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 4, x + 10, x + 14);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 4, x + 15, x + 15);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 5, x + 2, x + 2);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 5, x + 3, x + 5);
	set_display_attribute(FG_GREEN);
	draw_horizontal_line(y + 5, x + 6, x + 10);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 5, x + 11, x + 13);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 5, x + 14, x + 14);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 6, x + 1, x + 1);
	set_display_attribute(FG_GREEN);
	draw_horizontal_line(y + 6, x + 2, x + 14);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 6, x + 15, x + 15);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 7, x + 0, x + 0);
	set_display_attribute(FG_GREEN);
	draw_horizontal_line(y + 7, x + 1, x + 15);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 7, x + 3, x + 3);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 7, x + 13, x + 13);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 7, x + 16, x + 16);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 8, x + 0, x + 0);
	set_display_attribute(FG_GREEN);
	draw_horizontal_line(y + 8, x + 1, x + 15);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 8, x + 4, x + 12);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 8, x + 16, x + 16);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 9, x + 1, x + 1);
	set_display_attribute(FG_GREEN);
	draw_horizontal_line(y + 9, x + 2, x + 14);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 9, x + 15, x + 15);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 10, x + 2, x + 4);
	set_display_attribute(FG_GREEN);
	draw_horizontal_line(y + 10, x + 5, x + 11);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 10, x + 12, x + 14);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 11, x + 3, x + 3);
	set_display_attribute(FG_GREEN);
	draw_horizontal_line(y + 11, x + 4, x + 12);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 11, x + 5, x + 6);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 11, x + 10, x + 11);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 11, x + 13, x + 13);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 12, x + 1, x + 2);
	set_display_attribute(FG_GREEN);
	draw_horizontal_line(y + 12, x + 3, x + 13);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 12, x + 8, x + 8);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 12, x + 14, x + 15);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 13, x + 0, x + 0);
	set_display_attribute(FG_GREEN);
	draw_horizontal_line(y + 13, x + 1, x + 15);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 13, x + 7, x + 9);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 13, x + 2, x + 2);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 13, x + 14, x + 14);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 13, x + 16, x + 16);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 14, x + 0, x + 0);
	set_display_attribute(FG_GREEN);
	draw_horizontal_line(y + 14, x + 1, x + 15);
	set_display_attribute(FG_WHITE);
	draw_horizontal_line(y + 14, x + 7, x + 9);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 14, x + 4, x + 4);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 14, x + 6, x + 6);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 14, x + 10, x + 10);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 14, x + 12, x + 12);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 14, x + 16, x + 16);
	
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 15, x + 1, x + 3);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 15, x + 5, x + 5);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 15, x + 7, x + 9);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 15, x + 11, x + 11);
	set_display_attribute(FG_BLACK);
	draw_horizontal_line(y + 15, x + 13, x + 15);
}