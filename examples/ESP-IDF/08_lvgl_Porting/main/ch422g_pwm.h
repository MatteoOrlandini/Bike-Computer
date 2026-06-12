#ifndef _CH422G_PWM_H_
#define _CH422G_PWM_H_

#include <stdint.h>

void ch422g_pwm_init(void);
void ch422g_pwm_set_duty(uint8_t duty_percent); // 0–100
void ch422g_pwm_stop(void);

#endif