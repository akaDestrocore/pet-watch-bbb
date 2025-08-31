#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"
#include "hardware/uart.h"
#include "wifi.h"
#include "frame_receiver.h" 

#define MAX_WIFI_REINIT_TRIES    100

/**
 * @brief  The application entry point.
 * @return int
 */
int main()
{

    stdio_init_all();

    sleep_ms(2000);
    DBG("Pico 2W Image Forwarder Starting...\r\n");

    // Initialize UART for BBB communication with new frame protocol
    if (true != frame_receiver_init()) {
        DBG("Fatal: UART/Frame receiver init failed, halting.\r\n");
        while (true) {
            tight_loop_contents();
        }
    }

    if (true != wifi_init()) {
        DBG("Fatal: Wi-Fi init failed, halting.\r\n");
        while (true) {
            tight_loop_contents();
        }
    }

    DBG("Ready! Listening for frame-based transfers from BBB on UART%d\r\n", 
        BBB_UART_ID == uart0 ? 0 : 1);
    DBG("Protocol: [MAGIC:2][SIZE:4][CRC:2][DATA:N][CRC:2]\r\n");

    while (true) {
        // Check for frame transfers from BBB
        if (frame_receiver_process()) {
            // Forward a complete frame to PC
            uint8_t *image_data;
            uint32_t image_size;
            
            if (frame_receiver_get_data(&image_data, &image_size)) {
                DBG("Received image from BBB: %u bytes\r\n", image_size);
                
                // TODO: snapshot forwarding logic
                
                // Reset receiver for next transfer
                frame_receiver_reset();
            }
        }

        // Check if Wi-Fi is still working
        if (true != wifi_is_connected()) {
            DBG("Wi-Fi link down! Reinitializing…\r\n");
            wifi_deinit();
            
            bool reinit_ok = false;
            uint32_t backoff = INITIAL_RETRY_DELAY_MS;
            for (uint32_t i = 0; i < MAX_WIFI_REINIT_TRIES; ++i) {
                sleep_ms(backoff);
                if (true == wifi_init()) {
                    DBG("Wi-Fi back online after %u retries\r\n", i + 1);
                    reinit_ok = true;
                    break;
                }
                // Exponential backoff up to 10s
                backoff = (backoff * 2 > 10000) ? 10000 : backoff * 2; 
            }

            if (true != reinit_ok) {
                DBG("Failed %u reinit attempts, rebooting…\r\n", MAX_WIFI_REINIT_TRIES);
                reset_usb_boot(0, 0);
            }
            
            // Reinitialize frame receiver after WiFi recovery
            if (true != frame_receiver_init()) {
                DBG("Warning: Failed to reinit frame receiver after WiFi recovery\r\n");
            }
        }

        wifi_process();
        sleep_ms(10);
    }
    return 0;
}