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
 * @brief Water Pump Relay configuration
*/
#define RELAY_PULSE_MS    100      // Relay pulse duration

/** 
 * @brief Buzzer configuration
*/
#define BUZZER_PWM_FREQ   2000   // 2kHz tone
#define BUZZER_DUTY_CYCLE 50     // 50% duty cycle

/** 
 * @brief Alarm timing
*/
#define ALARM_DURATION_MS      5000   // 3 seconds total alarm
#define PUMP_TOGGLE_COUNT      3      // Number of pump toggles
#define PUMP_TOGGLE_DELAY_MS   1500   // Delay between pump toggles
#define BUZZER_BEEP_COUNT      10     // Number of beeps
#define BUZZER_BEEP_DELAY_MS   150    // Delay between buzzer beeps

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