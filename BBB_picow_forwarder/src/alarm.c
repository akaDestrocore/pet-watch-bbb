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

/* Private function prototypes -----------------------------------------------*/
static void set_servo_angle(float angle_degrees);
static void buzzer_on_off(bool enable);

/**
 * @brief Initialize alarm system (servo and buzzer)
 * @return true on success, false otherwise
 */
bool alarm_init(void) {
    // Initialize servo PWM
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    slice_servo = pwm_gpio_to_slice_num(SERVO_PIN);
    ch_servo = pwm_gpio_to_channel(SERVO_PIN);
    
    // Configure servo PWM for 50Hz (20ms period)
    // RP2350 default system clock is 150MHz
    pwm_config servo_config = pwm_get_default_config();
    
    // Set div to get 1MHz PWM clock
    pwm_config_set_clkdiv(&servo_config, 150.0f);
    
    // Set wrap to 20000 for 20ms period
    pwm_config_set_wrap(&servo_config, 19999);
    
    pwm_init(slice_servo, &servo_config, true);
    
    // Initialize buzzer PWM
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice_buzzer = pwm_gpio_to_slice_num(BUZZER_PIN);
    ch_buzzer = pwm_gpio_to_channel(BUZZER_PIN);
    
    // Configure buzzer PWM for 2kHz
    pwm_config buzzer_config = pwm_get_default_config();
    
    // 50MHz / (2000Hz * 1000) = 75 div
    pwm_config_set_clkdiv(&buzzer_config, 75.0f);
    pwm_config_set_wrap(&buzzer_config, 999);
    
    pwm_init(slice_buzzer, &buzzer_config, true);
    
    // Set initial states
    set_servo_angle(0.0f);
    buzzer_on_off(false);
    
    alarm_state = ALARM_IDLE;
    
    DBG("Alarm initialized: servo on GPIO%d, buzzer on GPIO%d", SERVO_PIN, BUZZER_PIN);
    
    // Test servo movement
    DBG("Testing servo movement.");
    set_servo_angle(180.0f);
    sleep_ms(500);
    set_servo_angle(90.0f);
    sleep_ms(500);
    set_servo_angle(0.0f);
    sleep_ms(500);
    
    // Test buzzer
    DBG("Testing buzzer.");
    buzzer_on_off(true);
    sleep_ms(200);
    buzzer_on_off(false);
    
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

        set_servo_angle(0.0f);
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
            set_servo_angle(90.0f);
            buzzer_on_off(false);
            DBG("Alarm sequence complete");
            return;
        }
        
        // Servo sweeping
        if (now - last_servo_update > SERVO_SWEEP_DELAY_MS) {
            switch (servo_pos) {
                case 0:
                    set_servo_angle(180.0f);
                    servo_pos = 1;
                    break;
                case 1:
                    set_servo_angle(0.0f);
                    servo_pos = 0;
                    break;
            }
            last_servo_update = now;
        }
        
        // Buzzer beeping
        if (now - last_buzzer_update > BUZZER_BEEP_DELAY_MS) {
            if (buzzer_beep_count < BUZZER_BEEP_COUNT * 2) {
                buzzer_on = !buzzer_on;
                buzzer_on_off(buzzer_on);
                buzzer_beep_count++;
            } else {
                // Reset for continuous beeping during alarm
                buzzer_beep_count = 0;
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
    set_servo_angle(90.0f);
    
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
    // Clamp angle to valid range
    if (angle_degrees < 0.0f) angle_degrees = 0.0f;
    if (angle_degrees > 180.0f) angle_degrees = 180.0f;
    
    // 1000us to 2000us
    uint16_t pulse_width_us = SERVO_MIN_PULSE + 
                             (uint16_t)((angle_degrees / 180.0f) * (SERVO_MAX_PULSE - SERVO_MIN_PULSE));
    
    pwm_set_chan_level(slice_servo, ch_servo, pulse_width_us);
    
    DBG("Servo: %.1fdeg -> %dus pulse", angle_degrees, pulse_width_us);
}

/**
 * @brief Turn buzzer on or off
 * @param enable true to turn on, false to turn off
 */
static void buzzer_on_off(bool enable) {
    if (enable) {
        // 50% duty cycle for 2kHz
        pwm_set_chan_level(slice_buzzer, ch_buzzer, 500);
    } else {
        pwm_set_chan_level(slice_buzzer, ch_buzzer, 0);
    }
}