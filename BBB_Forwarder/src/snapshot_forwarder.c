#include "snapshot_forwarder.h"

/* Private variables ---------------------------------------------------------*/
static Snapshot_Forwarder_t forwarder = {0};

/* Private function prototypes -----------------------------------------------*/
static err_t server_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t server_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len);
static void server_error_callback(void *arg, err_t err);
static err_t server_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

/**
 * @brief Initialize image forwarder for communication with server
 */
void snapshot_forwarder_init(void) {
    memset(&forwarder, 0, sizeof(Snapshot_Forwarder_t));
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
    memcpy(forwarder.image_buff, image_data, image_size);
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

/**
 * @brief TCP connected callback for server connection
 */
static err_t server_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (ERR_OK != err) {
        forwarder.forward_complete = false;
        DBG("Server connection error: %d", err);
        return err;
    }

    DBG("Connected to server, starting transfer of %u bytes", forwarder.image_size);
    
    // Build HTTP header
    char http_header[512];
    int header_len = snprintf(http_header,
                            sizeof(http_header),
                            "POST /image HTTP/1.1\r\n"
                            "Host: %s:%d\r\n"
                            "Content-Type: image/jpeg\r\n"
                            "Content-Length: %u\r\n"
                            "Connection: close\r\n"
                            "\r\n",
                            PC_SERVER_IP, PC_SERVER_PORT, forwarder.image_size);

    forwarder.header_len = header_len;
    memcpy(forwarder.http_header, http_header, header_len);
    
    // Try to send header
    uint16_t available = tcp_sndbuf(tpcb);
    if (available >= header_len) {
        err_t write_err = tcp_write(tpcb, forwarder.http_header, header_len, TCP_WRITE_FLAG_COPY);
        if (ERR_OK == write_err) {
            forwarder.header_sent = true;
            DBG("HTTP header sent (%d bytes)", header_len);
            // Flush
            tcp_output(tpcb);
        } else {
            DBG("Failed to write HTTP header: %d", write_err);
            return write_err;
        }
    } else {
        DBG("TCP buffer too small for header (%u < %d), will retry in sent callback", available, header_len);
    }
    
    return ERR_OK;
}

/**
 * @brief TCP sent callback
 */
static err_t server_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    DBG("TCP sent callback: %u bytes acknowledged", len);
    
    // Try to send header if not sent already
    if (!forwarder.header_sent) {
        u16_t available = tcp_sndbuf(tpcb);
        if (available >= forwarder.header_len) {
            err_t write_err = tcp_write(tpcb, forwarder.http_header, forwarder.header_len, TCP_WRITE_FLAG_COPY);
            if (ERR_OK == write_err) {
                forwarder.header_sent = true;
                DBG("HTTP header sent (%d bytes)", forwarder.header_len);
                tcp_output(tpcb);
                return ERR_OK;
            } else {
                DBG("Failed to write HTTP header: %d", write_err);
                forwarder.forward_complete = false;
                return write_err;
            }
        }
        return ERR_OK;
    }
    
    // Send image data after header already sent
    u16_t available = tcp_sndbuf(tpcb);
    u32_t remaining = forwarder.image_size - forwarder.bytes_sent;
    
    if (remaining > 0 && available > 0) {
        // Send as much as buffer allows
        u16_t to_send = (remaining < available) ? remaining : available;
        
        err_t write_err = tcp_write(tpcb, forwarder.image_buff + forwarder.bytes_sent, 
                                   to_send, 
                                   TCP_WRITE_FLAG_COPY);
        
        if (ERR_OK == write_err) {
            forwarder.bytes_sent += to_send;
            DBG("Sent %u bytes, total: %u/%u", to_send, forwarder.bytes_sent, forwarder.image_size);
            // Flush data
            tcp_output(tpcb);
            
            // Check if complete
            if (forwarder.bytes_sent >= forwarder.image_size) {
                DBG("All image data sent, waiting for server response");
            }
        } else {
            DBG("Failed to write image data: %d", write_err);
            forwarder.forward_complete = false;
            return write_err;
        }
    }
    
    return ERR_OK;
}

/**
 * @brief TCP error callback for server connection
 */
static void server_error_callback(void *arg, err_t err) {
    forwarder.forward_complete = false;
    forwarder.client_pcb = NULL;
    DBG("PC connection error: %d", err);
}

/**
 * @brief TCP receive callback for server response
 */
static err_t server_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (NULL == p) {
        // Server closed connection
        forwarder.forward_complete = true;
        DBG("Server closed connection, transfer complete");
        return ERR_OK;
    }

    // Check for alarm activation command in response
    char response[256];
    uint16_t copy_len = (p->tot_len < sizeof(response) - 1) ? p->tot_len : sizeof(response) - 1;
    pbuf_copy_partial(p, response, copy_len, 0);
    response[copy_len] = '\0';
    
    DBG("Received response from PC: %.100s", response);
    
    // Look for alarm command
    if (NULL != strstr(response, "ALARM")) {
        DBG("Received ALARM command from server");
        alarm_activate();
    }

    // Check for HTTP 200
    if (NULL != strstr(response, "200 OK") || NULL != strstr(response, "HTTP/1.1 200")) {
        forwarder.forward_complete = true;
        DBG("Transfer successful - received HTTP 200");
    }

    pbuf_free(p);
    tcp_recved(tpcb, p->tot_len);
    
    return ERR_OK;
}