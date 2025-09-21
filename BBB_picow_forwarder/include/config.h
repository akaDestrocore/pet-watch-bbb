#ifndef CONFIG_H
#define CONFIG_H

// Network configuration
#define WIFI_SSID               "SomeSSID"
#define WIFI_PASSWORD           "SomePassword"

// Server configuration
#define PC_SERVER_IP            "192.168.1.1XX"
#define PC_SERVER_PORT          7702

// BBB communication
#define BBB_UART_ID             uart1
#define BBB_UART_BAUD           460800

#define BBB_UART_TX_PIN         4
#define BBB_UART_RX_PIN         5

// Pin definitions
#define PUMP_RELAY_PIN          12
#define BUZZER_PIN              14

// Frame protocol configuration
#define MAX_IMAGE_SIZE          (100 * 1024)  // 100kB max for JPEG
#define IMAGE_TIMEOUT_MS        15000

// Retry configuration  
#define MAX_RETRY_COUNT         5
#define INITIAL_RETRY_DELAY_MS  1000

#define DEBUG 0
#ifdef DEBUG
    #define DBG(fmt, ...) printf("DBG: " fmt "\n", ##__VA_ARGS__)
#else
    #define DBG(fmt, ...)
#endif

#endif /* CONFIG_H */