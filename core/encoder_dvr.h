#ifndef ENCODER_DVR_H
#define ENCODER_DVR_H

#include <stdbool.h>
#include "pico/stdlib.h"

void encoder_init(void);
int encoder_get_delta(void);
bool encoder_is_button_pressed(void);
bool encoder_is_double_clicked(void);

void encoder_update_sw_state(void);
bool encoder_is_long_pressed(void);
uint8_t encoder_get_click_type(void); 

#endif // ENCODER_DRIVER_H