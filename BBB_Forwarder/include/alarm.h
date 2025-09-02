#ifndef ALARM_H
#define ALARM_H

#include "stdio.h"
#include <stdbool.h>
#include "pico/stdlib.h"
#include "config.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

/** 
 * @brief Pin definitions
*/
#define SERVO_PIN     2
#define BUZZER_PIN    3

/** 
 * @brief SG90 configuration
*/
#define SERVO_PWM_FREQ      50
#define SERVO_MIN_PULSE     1000    // 0 deg
#define SERVO_MAX_PULSE     2000    // 180 deg
#define SERVO_CENTER_PULSE  1500    // 90 deg

/** 
 * @brief Buzzer configuration
*/
#define BUZZER_PWM_FREQ   2000   // 2kHz tone
#define BUZZER_DUTY_CYCLE 50     // 50% duty cycle

/** 
 * @brief Alarm timing
*/
#define ALARM_DURATION_MS    3000  // 3 seconds total alarm
#define SERVO_SWEEP_COUNT    6
#define BUZZER_BEEP_COUNT    10
#define SERVO_SWEEP_DELAY_MS 250
#define BUZZER_BEEP_DELAY_MS 150

typedef enum {
    ALARM_IDLE,
    ALARM_ACTIVE,
    ALARM_COOLING_DOWN
} Alarm_State_t;

/** 
 * @brief Function prototypes
*/
// Initialize alarm system
bool alarm_init(void);
// Activate alarm sequence
void alarm_activate(void);
// Process alarm state machine
void alarm_process(void);
// Deinitialize alarm system
void alarm_deinit(void);
// Return alarm state
bool alarm_is_active(void);

#endif /* ALARM_H */