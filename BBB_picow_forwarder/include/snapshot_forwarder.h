#ifndef SNAPSHOT_FORWARDER_H
#define SNAPSHOT_FORWARDER_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "config.h"
#include "alarm.h"


/** 
 * @brief Snapshot forwarder handle structure
*/
typedef struct {
    struct tcp_pcb *server_pcb;             // Server PCB
    struct tcp_pcb *client_pcb;             // Client PCB
    uint8_t image_buff[MAX_IMAGE_SIZE];     // Snapshot data buffer
    uint32_t image_size;                    // Current snapshot size
    uint32_t received_bytes;                // Bytes received so far
    bool transfer_active;                   // Transfer in progress
    bool forward_complete;                  // Forwarding completed
    uint32_t transfer_start_time;           // Transfer start time
    uint32_t pending_sent;                  // Forwarding sent for server connection
    uint8_t http_header[512];               // Store HTTP header
    uint16_t header_len;                    // Header length
    bool header_sent;                       // Track if header was sent
    uint32_t bytes_sent;                    // Track image bytes sent
} Snapshot_Forwarder_t;

/** 
 * @brief Public function prototypes
*/
// Initialize snapshot forwarder for communication with server
void snapshot_forwarder_init(void);
// Send received snapshot to server
bool snapshot_forwarder_send_to_server(const uint8_t *image_data, uint32_t image_size);
// De-initialize forwarder
void snapshot_forwarder_deinit(void);

#endif /* SNAPSHOT_FORWARDER_H */