#ifndef CONFIG_H
#define CONFIG_H

// Network configuration
#define WIFI_SSID               "SomeSSID"
#define WIFI_PASSWORD           "SomePassword"

// Server configuration
#define PC_SERVER_IP            "192.168.2.107"
#define PC_SERVER_PORT          7654

// BBB communication
#define BBB_UART_ID             uart1
#define BBB_UART_BAUD           115200
#define BBB_UART_TX_PIN         4
#define BBB_UART_RX_PIN         5

// Frame protocol configuration
#define MAX_IMAGE_SIZE          (100 * 1024)  // 100kB max for JPEG

// Retry configuration  
#define MAX_RETRY_COUNT         5
#define INITIAL_RETRY_DELAY_MS  1000

#define DEBUG 1
#ifdef DEBUG
    #define DBG(fmt, ...) printf("DBG: " fmt "\n", ##__VA_ARGS__)
#else
    #define DBG(fmt, ...)
#endif

#endif /* CONFIG_H */