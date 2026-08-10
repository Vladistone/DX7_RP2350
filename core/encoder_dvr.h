#ifndef ENCODER_DRIVER_H
#define ENCODER_DRIVER_H

#include <stdbool.h>

int encoder_get_delta(void);
void encoder_init(void);
void encoder_update_sw_state(void);
bool encoder_is_single_clicked(void);
bool encoder_is_button_pressed(void);
bool encoder_is_double_clicked(void);
bool encoder_is_long_pressed(void);

#endif // ENCODER_DRIVER_H