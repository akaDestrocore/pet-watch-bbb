#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"
#include "hardware/uart.h"
#include "wifi.h"

#define MAX_WIFI_REINIT_TRIES    100

/**
 * @brief  The application entry point.
 * @return int
 */
int main()
{

    stdio_init_all();

    sleep_ms(2000);
    printf("Pico 2W Image Forwarder Starting...\r\n");

    if (true != wifi_init()) {
        printf("Fatal: Wi-Fi init failed, halting.\r\n");
        while (true) {
            tight_loop_contents();
        }
    }

    while (true) {
        // Check if Wi-Fi is still working
        if (true != wifi_is_connected()) {
            printf("Wi-Fi link down! Reinitializing…\r\n");
            wifi_deinit();
            
            bool reinit_ok = false;
            uint32_t backoff = INITIAL_RETRY_DELAY_MS;
            for (uint32_t i = 0; i < MAX_WIFI_REINIT_TRIES; ++i) {
                sleep_ms(backoff);
                if (true == wifi_init()) {
                    printf("Wi-Fi back online after %u retries\r\n", i + 1);
                    reinit_ok = true;
                    break;
                }
                // Exponential backoff up to 10s
                backoff = (backoff * 2 > 10000) ? 10000 : backoff * 2;
            }

            if (true != reinit_ok) {
                printf("Failed %u reinit attempts, rebooting…\r\n", MAX_WIFI_REINIT_TRIES);
                reset_usb_boot(0, 0);
            }
        }

        wifi_process();
        sleep_ms(10);
    }
    return 0;
}