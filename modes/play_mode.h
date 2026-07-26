#ifndef PLAY_MODE_H
#define PLAY_MODE_H

#include <stdint.h>
#include <stdbool.h>

void play_mode_render(void);
void play_mode_update(uint16_t touched, int enc_delta);

#endif // PLAY_MODE_H