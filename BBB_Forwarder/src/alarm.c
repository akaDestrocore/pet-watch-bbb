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

/**
 * @brief Initialize alarm system
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
    pwm_set_wrap(slice_servo, clock_freq / (div * SERVO_PWM_FREQ) - 1);

    // Initialize PWM for buzzer module
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice_buzzer = pwm_gpio_to_slice_num(BUZZER_PIN);
    ch_buzzer = pwm_gpio_to_channel(BUZZER_PIN);

    // Buzzer PWM to 2kHz
    uint32_t buzzer_div = clock_freq / (BUZZER_PWM_FREQ * (0xFFFF + 1));
    if (buzzer_div < 1) {
        buzzer_div = 1;
    }

    pwm_set_clkdiv(slice_buzzer, (float)buzzer_div);
    pwm_set_wrap(slice_buzzer, clock_freq / (buzzer_div * BUZZER_PWM_FREQ) - 1);

    // TODO: center the servo
    pwm_set_enabled(slice_servo, true);

    // TODO: buzzer state control here
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

        // TODO: servo at 0 deg
        buzzer_on = true;
    }
}

/**
 * @brief Process alarm state machine
 */
void alarm_process(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (ALARM_ACTIVE == alarm_state) {
        if (now - alarm_start_time > ALARM_DURATION_MS) {
            alarm_state = ALARM_COOLING_DOWN;
            // TODO: center servo
            DBG("Alarm sequence complete");
            return;
        }

        if (now - last_servo_update > SERVO_SWEEP_DELAY_MS) {
            switch (servo_pos) {
                case 0: {
                    // TODO: servo to 180
                    servo_pos = 1;
                    break;
                }
                
                case 1: {
                    // TODO: servo to 0
                    servo_pos = 0;
                    break;
                }
            }
            last_servo_update = now;
        }

        if (now - last_buzzer_update > BUZZER_BEEP_DELAY_MS) {
            if (buzzer_beep_count < BUZZER_BEEP_COUNT * 2) {
                buzzer_on = !buzzer_on;
                // TODO:
                buzzer_beep_count++;
            } else {
                buzzer_beep_count = 0;
                buzzer_on = false;
                // TODO:
            }
            last_buzzer_update = now;
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
 * @brief Deinitialize alarm system
 */
void alarm_deinit(void) {
    buzzer_on_off(false);
    // TODO: Center position
    
    pwm_set_enabled(slice_servo, false);
    pwm_set_enabled(slice_buzzer, false);
    
    alarm_state = ALARM_IDLE;
}