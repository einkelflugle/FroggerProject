/*
 * highscores.h
 *
 * Author: Max Miller
 */

uint8_t is_high_score(uint16_t score);

void save_high_score(uint16_t score, uint8_t *name);

void draw_high_scores(uint8_t x, uint8_t y);