#include "alarm.h"

/* Private variables ---------------------------------------------------------*/
static Alarm_State_t alarm_state = ALARM_IDLE;
static uint slice_buzzer;
static uint ch_buzzer;
static uint32_t alarm_start_time = 0;
static uint32_t last_pump_update = 0, last_buzzer_update = 0;
static uint32_t pump_pulse_start = 0;
static uint8_t pump_toggle_count  = 0;
static uint8_t buzzer_beep_count = 0;
static bool buzzer_on = false;
static bool pump_is_on = false;
static bool pump_pulse_active = false;
static bool initial_pump_done = false;

/* Private function prototypes -----------------------------------------------*/
static void start_pump_pulse(void);
static void process_pump_pulse(void);
static void buzzer_on_off(bool enable);

/**
 * @brief Initialize alarm system (water pump and buzzer)
 * @return true on success, false otherwise
 */
bool alarm_init(void) {
    // Initialize water pump pin
    gpio_init(PUMP_RELAY_PIN);
    gpio_set_dir(PUMP_RELAY_PIN, GPIO_IN);
    gpio_disable_pulls(PUMP_RELAY_PIN);
    sleep_ms(100);
    
    // Initialize buzzer PWM
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice_buzzer = pwm_gpio_to_slice_num(BUZZER_PIN);
    ch_buzzer = pwm_gpio_to_channel(BUZZER_PIN);
    
    // Configure buzzer PWM for 2kHz
    pwm_config buzzer_config = pwm_get_default_config();
    
    // 150MHz / (2000Hz * 1000) = 75 div
    pwm_config_set_clkdiv(&buzzer_config, 75.0f);
    pwm_config_set_wrap(&buzzer_config, 999);
    
    pwm_init(slice_buzzer, &buzzer_config, true);
    
    // Set initial state
    buzzer_on_off(false);
    pump_is_on = false;
    alarm_state = ALARM_IDLE;

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
        last_pump_update = alarm_start_time;
        last_buzzer_update = alarm_start_time;
        pump_toggle_count  = 0;
        buzzer_beep_count = 0;
        buzzer_on = false;

        DBG("ALARM ACTIVATED");
        buzzer_on_off(true);
        buzzer_on = true;
    }
}

/**
 * @brief Process alarm state machine
 */
void alarm_process(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (true == pump_pulse_active) {
        process_pump_pulse();
    }
    
    if (ALARM_ACTIVE == alarm_state) {
        // Check if alarm duration exceeded
        if (now - alarm_start_time > ALARM_DURATION_MS) {
            alarm_state = ALARM_COOLING_DOWN;
            buzzer_on_off(false);
            DBG("Alarm sequence complete");
            return;
        }
        
        // Initial pump activation
        if (!initial_pump_done && !pump_pulse_active) {
            if (true != pump_is_on) {
                DBG("Initial pump activation");
                start_pump_pulse();
            }
            initial_pump_done = true;
            last_pump_update = now;
        }
        
        // Handle subsequent pump toggles
        if (true == initial_pump_done && false == pump_pulse_active && 
            (now - last_pump_update > PUMP_TOGGLE_DELAY_MS)) {
            
            if (pump_toggle_count < PUMP_TOGGLE_COUNT) {
                DBG("Starting pump toggle %d/%d", pump_toggle_count + 1, PUMP_TOGGLE_COUNT);
                start_pump_pulse();
                pump_toggle_count++;
            }
            last_pump_update = now;
        }
        
        // Buzzer beeping
        if (now - last_buzzer_update > BUZZER_BEEP_DELAY_MS) {
            if (buzzer_beep_count < BUZZER_BEEP_COUNT * 2) {
                buzzer_on = !buzzer_on;
                buzzer_on_off(buzzer_on);
                buzzer_beep_count++;
            } else {
                // Reset for continuous beeping
                buzzer_beep_count = 0;
            }
            last_buzzer_update = now;
        }
    }
    else if (ALARM_COOLING_DOWN == alarm_state) {
        // Prevent continuous triggering
        if (now - alarm_start_time > ALARM_DURATION_MS + 2000) {
            alarm_state = ALARM_IDLE;
            pump_toggle_count = 0;
            initial_pump_done = false;
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
    if (true == pump_pulse_active) {
        gpio_set_dir(PUMP_RELAY_PIN, GPIO_IN);
        gpio_disable_pulls(PUMP_RELAY_PIN);
        pump_pulse_active = false;
    }
    
    buzzer_on_off(false);
    pwm_set_enabled(slice_buzzer, false);
    
    alarm_state = ALARM_IDLE;
    pump_toggle_count = 0;
    initial_pump_done = false;
}

/* Helper functions ----------------------------------------------------------*/

/**
 * @brief Toggle water pump by sending a brief LOW pulse to relay.
 *        This simulates a button press for the relay module.
 */
static void start_pump_pulse(void) {
    if (true == pump_pulse_active) {
        return;
    }
    
    DBG("Starting pump pulse (current state: %s)", pump_is_on ? "ON" : "OFF");
    
    // Start the pulse - set pin to output and drive low
    gpio_set_dir(PUMP_RELAY_PIN, GPIO_OUT);
    gpio_put(PUMP_RELAY_PIN, 0);
    
    pump_pulse_active = true;
    pump_pulse_start = to_ms_since_boot(get_absolute_time());
}

/**
 * @brief Process ongoing pump pulse
 */
static void process_pump_pulse(void) {
    if (true != pump_pulse_active) {
        return;
    }
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    if (now - pump_pulse_start >= RELAY_PULSE_MS) {
        // End the pulse
        gpio_set_dir(PUMP_RELAY_PIN, GPIO_IN);
        gpio_disable_pulls(PUMP_RELAY_PIN);
        
        pump_pulse_active = false;
        pump_is_on = !pump_is_on;
        
        DBG("Pump pulse complete");
    }
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