#include "frame_receiver.h"

/* Private variables ---------------------------------------------------------*/
static Frame_Handle_t frame_rec = {0};

/* Private function prototypes -----------------------------------------------*/
static bool verify_frame_crc(void);
static uint16_t calculate_crc16(const uint8_t* data, size_t len);
static void BBB_UART_IRQHandler(void);
static bool parse_frame_header(void);
static bool rx_buff_available(void);
static uint8_t rx_buff_get(void);
static void rx_buff_put(uint8_t byte);


/**
 * @brief Initialize frame receiver with UART
 * @return true on success, false otherwise
 */
bool frame_receiver_init(void){
    uart_init(BBB_UART_ID, BBB_UART_BAUD);
    gpio_set_function(BBB_UART_RX_PIN, GPIO_FUNC_UART);
    gpio_set_function(BBB_UART_TX_PIN, GPIO_FUNC_UART);

    // UART peripheral configuration
    uart_set_format(BBB_UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(BBB_UART_ID, true);

    // Full reset states
    memset(&frame_rec, 0, sizeof(Frame_Handle_t));
    frame_rec.state = FRAME_IDLE;

    // UART IRQ Handler registration
    int UART_IRQ = BBB_UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, BBB_UART_IRQHandler);
    irq_set_enabled(UART_IRQ, true);
    uart_set_irqs_enabled(BBB_UART_ID, true, false);

    DBG("UART%d initialized for frame protocol at %d baud", BBB_UART_ID == uart0 ? 0 : 1, BBB_UART_BAUD);

    return true;
}

/**
 * @brief Process frame receiver state machine
 * @return true if frame is complete, false otherwise
 */
bool frame_receiver_process(void) {
    // Get current time
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Timeout check
    if (FRAME_IDLE != frame_rec.state && 0 != frame_rec.last_activity) {
        if (now - frame_rec.last_activity > FRAME_TIMEOUT_MS) {
            frame_receiver_reset();
            DBG("Frame timeout.");
            return false;
        }
    }

    // While data is available in ring buffer
    while (true == rx_buff_available()) {
        uint8_t byte = rx_buff_get();

        switch(frame_rec.state) {
            case FRAME_IDLE: {
                // Check start magic
                if (FRAME_START_MAGIC_0 == byte) {
                    frame_rec.header_buffer[0] = byte;
                    frame_rec.frame_index = 1;
                    frame_rec.state = FRAME_RECEIVING_HEADER;
                }
                break;
            }

            case FRAME_RECEIVING_HEADER: {
                frame_rec.header_buffer[frame_rec.frame_index++] = byte;

                if (2 == frame_rec.frame_index && FRAME_START_MAGIC_1 != byte) {
                    frame_rec.state = FRAME_IDLE;
                    frame_rec.frame_index = 0;
                    if (byte == FRAME_START_MAGIC_0) {
                        frame_rec.header_buffer[0] = byte;
                        frame_rec.frame_index = 1;
                        frame_rec.state = FRAME_RECEIVING_HEADER;
                    }
                } else if (frame_rec.frame_index >= FRAME_HEADER_SIZE) {
                    if (parse_frame_header()) {
                        frame_rec.state = FRAME_RECEIVING_DATA;
                        frame_rec.frame_index = 0; 
                    } else {
                        frame_rec.state = FRAME_ERROR;
                    }
                }
                break;
            }

            case FRAME_RECEIVING_DATA: {
                if (frame_rec.frame_index < frame_rec.expected_data_size) {
                    frame_rec.image_buffer[frame_rec.frame_index++] = byte;
                } else {
                    uint32_t crc_index = frame_rec.frame_index - frame_rec.expected_data_size;
                    if (crc_index < FRAME_CRC_SIZE) {
                        frame_rec.header_buffer[crc_index] = byte; 
                        frame_rec.frame_index++;
                        
                        if (FRAME_CRC_SIZE - 1 == crc_index) {
                            frame_rec.state = FRAME_PROCESSING;
                        }
                    }
                }
                break;
            }
            
            default:
                break;
        }
    }

    // Handle frame processing
    if (FRAME_PROCESSING == frame_rec.state) {
        if (true == verify_frame_crc()) {
            frame_rec.received_bytes = frame_rec.expected_data_size;
            frame_rec.state = FRAME_COMPLETE;
            frame_rec.frame_ready = true;
            DBG("Frame complete: %u bytes received", frame_rec.received_bytes);
            return true;
        } else {
            frame_rec.state = FRAME_ERROR;
            DBG("Frame CRC verification failed");
        }
    }

    if (FRAME_ERROR == frame_rec.state) {
        frame_receiver_reset();
    }

    return frame_rec.state == FRAME_COMPLETE;
}

/**
 * @brief Get received image data
 * @param data Pointer to receive data pointer
 * @param size Pointer to receive data size
 * @return true if data is available, false otherwise
 */
bool frame_receiver_get_data(uint8_t **data, uint32_t *size) {
    if (frame_rec.state == FRAME_COMPLETE && frame_rec.frame_ready && frame_rec.received_bytes > 0) {
        *data = frame_rec.image_buffer;
        *size = frame_rec.received_bytes;
        return true;
    }
    return false;
}

/**
 * @brief Reset frame receiver for next transfer
 */
void frame_receiver_reset(void) {
    frame_rec.state = FRAME_IDLE;
    frame_rec.frame_index = 0;
    frame_rec.expected_data_size = 0;
    frame_rec.received_bytes = 0;
    frame_rec.last_activity = 0;
    frame_rec.frame_ready = false;
    frame_rec.rx_head = 0;
    frame_rec.rx_tail = 0;
    
    // Clear buffers
    memset(frame_rec.header_buffer, 0, sizeof(frame_rec.header_buffer));
}

/**
 * @brief Deinitialize frame receiver
 */
void frame_receiver_deinit(void) {
    // Disable UART IRQs
    int UART_IRQ = BBB_UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_enabled(UART_IRQ, false);
    uart_set_irqs_enabled(BBB_UART_ID, false, false);
    
    memset(&frame_rec, 0, sizeof(Frame_Handle_t));
}

/**
 * @brief UART IRQ handler for receiving frame data
 */
static void BBB_UART_IRQHandler(void) {
    while (uart_is_readable(BBB_UART_ID)) {
        uint8_t byte = uart_getc(BBB_UART_ID);
        frame_rec.last_activity = to_ms_since_boot(get_absolute_time());
        rx_buff_put(byte);
    }
}

/* Helper functions ----------------------------------------------------------*/

/**
 * @brief Verify frame CRC
 * @return true if CRC is valid, false otherwise
 */
static bool verify_frame_crc(void) {
    // Extract frame CRC from header buffer
    uint16_t frame_crc = (uint16_t)frame_rec.header_buffer[0] | ((uint16_t)frame_rec.header_buffer[1] << 8);
    
    // Calculate CRC
    uint16_t calculated_crc = calculate_crc16(frame_rec.image_buffer, frame_rec.expected_data_size);
    
    if (frame_crc != calculated_crc) {
        DBG("Frame CRC error: received %04x, calculated %04x", frame_crc, calculated_crc);
        return false;
    }
    
    return true;
}

/**
 * @brief Calculate CRC-16 checksum (CCITT polynomial)
 * @param data Pointer to data buffer
 * @param len Length of data buffer
 * @return Calculated CRC-16 value
 */
static uint16_t calculate_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief Parse frame header and validate
 * @return true if header is valid, false otherwise
 */
static bool parse_frame_header(void) {
    // Verify magic bytes
    if (FRAME_START_MAGIC_0 != frame_rec.header_buffer[0] || 
        FRAME_START_MAGIC_1 != frame_rec.header_buffer[1]) {
        DBG("Invalid frame magic: %02x %02x", frame_rec.header_buffer[0], frame_rec.header_buffer[1]);
        return false;
    }
    
    // Extract data size in little endian
    uint32_t data_size = (uint32_t)frame_rec.header_buffer[2] |
                        ((uint32_t)frame_rec.header_buffer[3] << 8) |
                        ((uint32_t)frame_rec.header_buffer[4] << 16) |
                        ((uint32_t)frame_rec.header_buffer[5] << 24);
    
    // Extract header CRC in little endian
    uint16_t header_crc = (uint16_t)frame_rec.header_buffer[6] |
                         ((uint16_t)frame_rec.header_buffer[7] << 8);
    
    // Verify header CRC
    uint16_t calculated_crc = calculate_crc16(frame_rec.header_buffer, 6);
    if (header_crc != calculated_crc) {
        DBG("Header CRC error: received %04x, calculated %04x", header_crc, calculated_crc);
        return false;
    }
    
    // Validate data size
    if (0 == data_size || data_size > FRAME_MAX_DATA_SIZE) {
        DBG("Invalid data size: %u bytes (max %u)", data_size, FRAME_MAX_DATA_SIZE);
        return false;
    }
    
    frame_rec.expected_data_size = data_size;
    
    DBG("Frame header parsed: data_size=%u, header_crc=%04x", data_size, header_crc);
    return true;
}

/**
 * @brief Check if data is available in ring buffer
 * @return true if data available, false otherwise
 */
static bool rx_buff_available(void) {
    return frame_rec.rx_head != frame_rec.rx_tail;
}

/**
 * @brief Get byte from circular buffer
 * @return Next byte from buffer
 */
static uint8_t rx_buff_get(void) {
    uint8_t byte = frame_rec.rx_buff[frame_rec.rx_tail];
    frame_rec.rx_tail = (frame_rec.rx_tail + 1) % sizeof(frame_rec.rx_buff);
    return byte;
}

/**
 * @brief Put byte into circular buffer
 * @param byte Byte to store
 */
static void rx_buff_put(uint8_t byte) {
    uint16_t next_head = (frame_rec.rx_head + 1) % sizeof(frame_rec.rx_buff);
    if (next_head != frame_rec.rx_tail) {
        // Buffer not full
        frame_rec.rx_buff[frame_rec.rx_head] = byte;
        frame_rec.rx_head = next_head;
    }

}