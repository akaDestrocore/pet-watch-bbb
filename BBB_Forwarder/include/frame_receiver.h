#ifndef FRAME_RECEIVER_H
#define FRAME_RECEIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "config.h"

/** 
 * @brief Protocol constants
*/
#define FRAME_START_MAGIC_0     0xAA
#define FRAME_START_MAGIC_1     0x55
#define FRAME_HEADER_SIZE       8
#define FRAME_CRC_SIZE          2
#define FRAME_MIN_SIZE          (FRAME_HEADER_SIZE + FRAME_CRC_SIZE)
#define FRAME_MAX_DATA_SIZE     MAX_IMAGE_SIZE
#define FRAME_TIMEOUT_MS        5000

/** 
 * @brief Frame receiver states
*/
typedef enum {
    FRAME_IDLE,
    FRAME_RECEIVING_HEADER,
    FRAME_RECEIVING_DATA,
    FRAME_PROCESSING,
    FRAME_COMPLETE,
    FRAME_ERROR
} Frame_State_t;

/** 
 * @brief Frame receiver handle structure
*/
typedef struct {
    uint8_t header_buffer[FRAME_HEADER_SIZE];   // Static header buffer
    uint8_t image_buffer[MAX_IMAGE_SIZE];       // Static image buffer
    Frame_State_t state;                        // Current state of the receiver 
    uint32_t frame_index;                       // Current position in reception
    uint32_t expected_data_size;                // Expected data size from header
    uint32_t received_bytes;                    // Actual received data bytes
    uint32_t last_activity;                     // Last activity timestamp
    bool frame_ready;                           // Flag indicating complete frame is ready
    uint8_t rx_buff[512];                       // Ring buffer for UART data
    uint16_t rx_head;                           // Head pointer for ring buffer
    uint16_t rx_tail;                           // Tail pointer for ring buffer
} Frame_Handle_t;

/** 
 * @brief Public function prototypes
*/
// Frame receiver UART initialization
bool frame_receiver_init(void);
// Main function for frame receiver state machine
bool frame_receiver_process(void);

#endif /* FRAME_RECEIVER_H */