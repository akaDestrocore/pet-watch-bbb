#include "snapshot_forwarder.h"

/* Private variables ---------------------------------------------------------*/
static Snapshot_Forwarder_t forwarder = {0};

/* Private function prototypes -----------------------------------------------*/

/**
 * @brief Initialize image forwarder for communication with server
 */
void snapshot_forwarder_init(void) {
    memset(forwarder, 0, sizeof(Snapshot_Forwarder_t));
}

/**
 * @brief Send snapshot data to server
 * @param image_data Pointer to image data
 * @param image_size Size of image data in bytes
 * @return true on success, false otherwise
 */
bool snapshot_forwarder_send_to_server(const uint8_t *image_data, uint32_t image_size) {
    if (NULL == image_data || 0 == image_size || image_size > MAX_IMAGE_SIZE) {
        DBG("Invalid image data: size=%u", image_size);
        return false;
    }

    // Copy data to inner buffer
    memcpy(forwarder.image_buffer, image_data, image_size);
    forwarder.image_size = image_size;
    forwarder.bytes_sent = 0;
    forwarder.header_sent = false;

    ip_addr_t server_ip;
    server_ip.addr = ipaddr_addr(PC_SERVER_IP);

    // Create new TCP protocol control block for client
    cyw43_arch_lwip_begin();
    forwarder.client_pcb = tcp_new();
    cyw43_arch_lwip_end();

    if (NULL == forwarder.client_pcb) {
        DBG("Failed to create client PCB");
        return false;
    }

    // Set up callbacks
    tcp_arg(forwarder.client_pcb, NULL);
    tcp_err(forwarder.client_pcb, server_error_callback);
    tcp_recv(forwarder.client_pcb, server_recv_callback);
    tcp_sent(forwarder.client_pcb, server_sent_callback);

    forwarder.forward_complete = false;
    forwarder.transfer_start_time = to_ms_since_boot(get_absolute_time());

    // Connect to server
    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(forwarder.client_pcb, &server_ip, PC_SERVER_PORT, server_connected_callback);
    cyw43_arch_lwip_end();

    if (ERR_OK != err) {
        DBG("Failed to connect to server: %d", err);
        cyw43_arch_lwip_begin();
        tcp_close(forwarder.client_pcb);
        cyw43_arch_lwip_end();
        forwarder.client_pcb = NULL;
        return false;
    }

    // Wait for completion
    uint32_t timeout = forwarder.transfer_start_time + IMAGE_TIMEOUT_MS;
    while (true != forwarder.forward_complete && to_ms_since_boot(get_absolute_time()) < timeout) {
        cyw43_arch_poll();
        sleep_ms(10);
    }

    // Clean up connection
    if (NULL != forwarder.client_pcb) {
        cyw43_arch_lwip_begin();
        tcp_close(forwarder.client_pcb);
        cyw43_arch_lwip_end();
        forwarder.client_pcb = NULL;
    }

    if (true != forwarder.forward_complete) {
        DBG("Image forwarding timed out");
        return false;
    }

    return true;
}

/**
 * @brief Deinitialize image forwarder
 */
void snapshot_forwarder_deinit(void) {
    if (NULL != forwarder.client_pcb) {
        cyw43_arch_lwip_begin();
        tcp_close(forwarder.client_pcb);
        cyw43_arch_lwip_end();
        forwarder.client_pcb = NULL;
    }
    
    // Reset forwarder state
    memset(&forwarder, 0, sizeof(forwarder));
}

// TOFO: add callbacks 