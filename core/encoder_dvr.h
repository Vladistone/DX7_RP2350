#ifndef ENCODER_DRIVER_H
#define ENCODER_DRIVER_H

#include <stdbool.h>

void encoder_init(void);
int encoder_get_delta(void);
bool encoder_is_button_pressed(void);
bool encoder_is_double_clicked(void);

#endif // ENCODER_DRIVER_H