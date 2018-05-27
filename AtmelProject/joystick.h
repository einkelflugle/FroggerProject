/*
 * joystick.h
 *
 * Author: Max Miller
 */

void init_joystick(void);

uint8_t poll_joystick_direction(void);

uint8_t joystick_move(uint8_t joystick_direction);