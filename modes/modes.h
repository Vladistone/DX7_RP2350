#ifndef MODES_H
#define MODES_H

#include <stdint.h>

typedef enum {
    MODE_PLAYBACK,
    MODE_FILE_SELECT,
    MODE_USB_MIDI,
    MODE_SYSTEM_CONFIG,
    MODE_COUNT
} AppModeState;

extern AppModeState g_current_mode;

void play_mode_render(void);
void play_mode_update(uint16_t touched, int enc_delta);

void sd_review_render(void);
void sd_review_update(uint16_t touched, int enc_delta);

void midi_bridge_render(void);
void midi_bridge_update(uint16_t touched, int enc_delta);

void system_mode_render(void);
void system_mode_update(uint16_t touched, int enc_delta);

#endif // MODES_H