/*
 * sound_effects.c
 *
 * Generates a few different sounds for events in the game through a piezo buzzer.
 * Hardware:
 * - Piezo buzzer connected to OC1B pin (port D, pin 4) and ground (0V)
 * - Switch S7 connected to port D, pin 7 for mute toggle
 * - Switch S6 connected to port D, pin 6 for volume high/low toggle
 *
 * Author: maxcmiller
 */

#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "terminalio.h"
#include "timer0.h"

#define SOUND_QUEUE_SIZE 10
uint16_t sound_queue_frequencies[SOUND_QUEUE_SIZE];
uint16_t sound_queue_durations[SOUND_QUEUE_SIZE];
uint32_t sound_queue_begin_times[SOUND_QUEUE_SIZE];
uint8_t sound_queue_length = 0;

// For a given frequency (Hz), return the clock period (in terms of the
// number of clock cycles of a 1MHz clock)
static uint16_t freq_to_clock_period(uint16_t freq) {
	return (1000000UL / freq);	// UL makes the constant an unsigned long (32 bits)
	// and ensures we do 32 bit arithmetic, not 16
}

// Return the width of a pulse (in clock cycles) given a duty cycle (%) and
// the period of the clock (measured in clock cycles)
static uint16_t duty_cycle_to_pulse_width(float dutycycle, uint16_t clockperiod) {
	return (dutycycle * clockperiod) / 100;
}

static void play_sound(uint16_t freq) {
	uint16_t clockperiod;
	uint16_t pulsewidth;
	float dutycycle;
	
	// Don't play sound if the game is muted (ie. switch 7 is in off position, 0)
	if ((PIND & 0b10000000) == 0) {
		return;
	}
	
	// Determine the duty cycle based on the volume high/low switch (switch 6)
	if ((PIND & 0b01000000) == 0) {
		// Quieter output, lower duty cycle
		dutycycle = 1;		// %
	} else {
		dutycycle = 5;
	}
	
	clockperiod = freq_to_clock_period(freq);
	pulsewidth = duty_cycle_to_pulse_width(dutycycle, clockperiod);
	
	// Set the maximum count value for timer/counter 1 to be one less than the clockperiod
	OCR1A = clockperiod - 1;
	
	// Set the count compare value based on the pulse width. The value will be 1 less
	// than the pulse width - unless the pulse width is 0.
	if(pulsewidth == 0) {
		OCR1B = 0;
		} else {
		OCR1B = pulsewidth - 1;
	}
	
	// Set up timer/counter 1 for Fast PWM, counting from 0 to the value in OCR1A
	// before reseting to 0. Count at 1MHz (CLK/8).
	// Configure output OC1B to be clear on compare match and set on timer/counter
	// overflow (non-inverting mode).
	TCCR1A = (1 << COM1B1) | (0 <<COM1B0) | (1 <<WGM11) | (1 << WGM10);
	TCCR1B = (1 << WGM13) | (1 << WGM12) | (0 << CS12) | (1 << CS11) | (0 << CS10);
	
	// Buzzer should now be buzzing
}

void init_sound_effects(void) {
	// Make pin OC1B be an output (port D, pin 4)
	DDRD = (1<<4);
}

void stop_sound(void) {
	TCCR1A = 0; // Reverts OC1A/OC1B to normal port operation, disconnecting the buzzer
}

/* Gets called every time the main game loop runs */
void update_sound_effects(void) {
	uint32_t current_time = get_current_time();
	
	if (sound_queue_length == 0) {
		// No sounds are scheduled to be played, turn off the buzzer
		stop_sound();
	} else {
		if (sound_queue_begin_times[0] + sound_queue_durations[0] <= current_time) {
			// Stop the currently playing sound, it has finished
			stop_sound();
			
			// Shift all elements towards the front, removing the first element from the queue
			for (uint8_t i = 1; i < sound_queue_length; i++) {
				sound_queue_frequencies[i-1] = sound_queue_frequencies[i];
				sound_queue_begin_times[i-1] = sound_queue_begin_times[i];
				sound_queue_durations[i-1] = sound_queue_durations[i];
			}
			sound_queue_length--;
		} else {
			// Play the next sound if it is scheduled to begin now
			if (sound_queue_begin_times[0] <= current_time) {
				play_sound(sound_queue_frequencies[0]);
			}
		}
	}
}

void play_sound_new_level(void) {
	sound_queue_length = 0; // Clear the current sound queue
	
	sound_queue_frequencies[sound_queue_length] = 2000;
	sound_queue_durations[sound_queue_length] = 200;
	sound_queue_begin_times[sound_queue_length] = get_current_time();
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 2500;
	sound_queue_durations[sound_queue_length] = 200;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 200;
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 3000;
	sound_queue_durations[sound_queue_length] = 200;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 400;
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 2500;
	sound_queue_durations[sound_queue_length] = 200;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 600;
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 2000;
	sound_queue_durations[sound_queue_length] = 400;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 800;
	sound_queue_length += 1;
}

void play_sound_death(void) {
	sound_queue_frequencies[sound_queue_length] = 100;
	sound_queue_durations[sound_queue_length] = 300;
	sound_queue_begin_times[sound_queue_length] = get_current_time();
	sound_queue_length += 1;
}

void play_sound_game_over(void) {
	sound_queue_length = 0;
	
	sound_queue_frequencies[sound_queue_length] = 1000;
	sound_queue_durations[sound_queue_length] = 100;
	sound_queue_begin_times[sound_queue_length] = get_current_time();
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 800;
	sound_queue_durations[sound_queue_length] = 100;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 500;
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 600;
	sound_queue_durations[sound_queue_length] = 300;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 1000;
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 800;
	sound_queue_durations[sound_queue_length] = 250;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 1300;
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 1000;
	sound_queue_durations[sound_queue_length] = 250;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 1550;
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 800;
	sound_queue_durations[sound_queue_length] = 250;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 1800;
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 600;
	sound_queue_durations[sound_queue_length] = 250;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 2050;
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 800;
	sound_queue_durations[sound_queue_length] = 250;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 2300;
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 600;
	sound_queue_durations[sound_queue_length] = 300;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 2550;
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 400;
	sound_queue_durations[sound_queue_length] = 500;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 2850;
	sound_queue_length += 1;
}

void play_sound_reached_riverbank(void) {
	sound_queue_length = 0;
	
	sound_queue_frequencies[sound_queue_length] = 500;
	sound_queue_durations[sound_queue_length] = 200;
	sound_queue_begin_times[sound_queue_length] = get_current_time();
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 500;
	sound_queue_durations[sound_queue_length] = 100;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 400;
	sound_queue_length += 1;
	
	sound_queue_frequencies[sound_queue_length] = 2000;
	sound_queue_durations[sound_queue_length] = 500;
	sound_queue_begin_times[sound_queue_length] = get_current_time() + 500;
	sound_queue_length += 1;
	
	update_sound_effects();
}

void play_sound_frog_move(void) {
	sound_queue_frequencies[sound_queue_length] = 900;
	sound_queue_durations[sound_queue_length] = 50;
	sound_queue_begin_times[sound_queue_length] = get_current_time();
	sound_queue_length += 1;
}