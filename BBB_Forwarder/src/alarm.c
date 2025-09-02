#include "alarm.h"

/* Private variables ---------------------------------------------------------*/
static Alarm_State_t alarm_state = ALARM_IDLE;
static uint slice_servo, slice_buzzer;
static uint ch_servo, ch_buzzer;
static uint32_t alarm_start_time = 0;
static uint32_t last_servo_update, last_buzzer_update = 0;
static uint8_t servo_pos = 0;
static uint8_t buzzer_beep_count = 0;
static bool buzzer_on = false;
static uint16_t servo_wrap_value = 0;
static uint16_t buzzer_wrap_value = 0;

/* Private function prototypes -----------------------------------------------*/
static void set_servo_angle(float angle_degrees);
static void buzzer_on_off(bool enable);
static uint16_t calculate_servo_pwm_level(uint16_t pulse_width_us);

/**
 * @brief Initialize alarm system (servo and buzzer)
 * @return true on success, false otherwise
 */
bool alarm_init(void) {
    // Initialize PWM for servo
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    slice_servo = pwm_gpio_to_slice_num(SERVO_PIN);
    ch_servo = pwm_gpio_to_channel(SERVO_PIN);
    
    // Set servo to 50 Hz
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t div = clock_freq / (SERVO_PWM_FREQ * (0xFFFF + 1));
    if (div < 1) {
        div = 1;
    }
    
    pwm_set_clkdiv(slice_servo, (float)div);
    servo_wrap_value = clock_freq / (div * SERVO_PWM_FREQ) - 1;
    pwm_set_wrap(slice_servo, servo_wrap_value);
    
    // Initialize PWM for buzzer module
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice_buzzer = pwm_gpio_to_slice_num(BUZZER_PIN);
    ch_buzzer = pwm_gpio_to_channel(BUZZER_PIN);
    
    // Buzzer PWM to 2kHz
    uint32_t buzzer_div = clock_freq / (BUZZER_PWM_FREQ * (0xFFFF + 1));
    if (buzzer_div < 1) buzzer_div = 1;
    
    pwm_set_clkdiv(slice_buzzer, (float)buzzer_div);
    buzzer_wrap_value = clock_freq / (buzzer_div * BUZZER_PWM_FREQ) - 1;
    pwm_set_wrap(slice_buzzer, buzzer_wrap_value);
    
    set_servo_angle(90.0);
    pwm_set_enabled(slice_servo, true);

    buzzer_on_off(false);
    pwm_set_enabled(slice_buzzer, true);
    
    alarm_state = ALARM_IDLE;
    
    DBG("Alarm ready: servo on GPIO%d, buzzer on GPIO%d", SERVO_PIN, BUZZER_PIN);
    return true;
}

/**
 * @brief Activate alarm sequence
 */
void alarm_activate(void) {
    if (ALARM_IDLE == alarm_state) {
        alarm_state = ALARM_ACTIVE;
        alarm_start_time = to_ms_since_boot(get_absolute_time());
        last_servo_update = alarm_start_time;
        last_buzzer_update = alarm_start_time;
        servo_pos = 0;
        buzzer_beep_count = 0;
        buzzer_on = false;

        DBG("ALARM ACTIVATED");

        set_servo_angle(0.0);
        buzzer_on_off(true);
        buzzer_on = true;
    }
}

/**
 * @brief Process alarm state machine
 */
void alarm_process(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    if (ALARM_ACTIVE == alarm_state) {
        // Check if alarm duration exceeded
        if (now - alarm_start_time > ALARM_DURATION_MS) {
            alarm_state = ALARM_COOLING_DOWN;
            set_servo_angle(90.0);
            buzzer_on_off(false);
            DBG("Alarm sequence complete");
            return;
        }
        
        if (now - last_servo_update > SERVO_SWEEP_DELAY_MS) {
            switch (servo_pos) {
                case 0: {
                    set_servo_angle(180.0);
                    servo_pos = 1;
                    break;
                }

                case 1: {
                    set_servo_angle(0.0);
                    servo_pos = 0;
                    break;
                }
            }
            last_servo_update = now;
        }
        
        if (now - last_buzzer_update > BUZZER_BEEP_DELAY_MS) {
            if (buzzer_beep_count < BUZZER_BEEP_COUNT * 2) {
                buzzer_on = !buzzer_on;
                buzzer_on_off(buzzer_on);
                buzzer_beep_count++;
            } else {
                buzzer_beep_count = 0;
                buzzer_on = false;
                buzzer_on_off(false);
            }
            last_buzzer_update = now;
        }
    }
    else if (ALARM_COOLING_DOWN == alarm_state) {
        // Prevent continuous triggering
        if (now - alarm_start_time > ALARM_DURATION_MS + 2000) {
            alarm_state = ALARM_IDLE;
            DBG("Alarm system ready");
        }
    }
}

/**
 * @brief Check if alarm is currently active
 * @return true if alarm is running, false otherwise
 */
bool alarm_is_active(void) {
    return (ALARM_ACTIVE == alarm_state);
}

/**
 * @brief De-initialize alarm system
 */
void alarm_deinit(void) {
    buzzer_on_off(false);
    set_servo_angle(90.0);
    
    pwm_set_enabled(slice_servo, false);
    pwm_set_enabled(slice_buzzer, false);
    
    alarm_state = ALARM_IDLE;
}

/* Helper functions ----------------------------------------------------------*/

/**
 * @brief Set servo angle in degrees (0-180)
 * @param angle_degrees Desired angle
 */
static void set_servo_angle(float angle_degrees) {
    if (angle_degrees < 0.0) {
        angle_degrees = 0.0;
    }
    if (angle_degrees > 180.0) {
        angle_degrees = 180.0;
    }
    
    // Calculate pulse width
    uint16_t pulse_width = SERVO_MIN_PULSE + 
                          (uint16_t)((angle_degrees / 180.0) * (SERVO_MAX_PULSE - SERVO_MIN_PULSE));
    
    // Convert to PWM
    uint16_t pwm_level = calculate_servo_pwm_level(pulse_width);
    pwm_set_chan_level(slice_servo, ch_servo, pwm_level);
}

/**
 * @brief Turn buzzer on or off
 * @param enable true to turn on, false to turn off
 */
static void buzzer_on_off(bool enable) {
    if (true == enable) {
        uint16_t level = (buzzer_wrap_value * BUZZER_DUTY_CYCLE) / 100;
        pwm_set_chan_level(slice_buzzer, ch_buzzer, level);
    } else {
        pwm_set_chan_level(slice_buzzer, ch_buzzer, 0);
    }
}

/**
 * @brief Calculate PWM level for servo pulse width
 * @param pulse_width_us Pulse width in microseconds
 * @return PWM level value
 */
static uint16_t calculate_servo_pwm_level(uint16_t pulse_width_us) {
    uint32_t period_us = 1000000 / SERVO_PWM_FREQ;
    return (uint16_t)((pulse_width_us * servo_wrap_value) / period_us);
}