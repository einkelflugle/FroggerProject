/*
 * sevenseg.h
 *
 * Author: maxcmiller
 */ 

#ifndef SEVENSEG_H_
#define SEVENSEG_H_

#include <stdint.h>

/* Displays a time remaining value on the seven segment display. */
void display_ssd(uint32_t time_remaining);

/* Initialises the seven segment display. */
void init_sevenseg(void);

#endif