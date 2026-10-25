
#include "ESP8266_STM32.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

/* =========================================================
 * Configuration
 * ========================================================= */

#ifndef ENABLE_DEBUG_LOG
#define ENABLE_DEBUG_LOG 1
#endif

#define ESP_RX_BUFFER_SIZE       2048U
#define ESP_LOG_BUFFER_SIZE      320U
#define ESP_AT_TIMEOUT_MS        3000U
#define ESP_RESET_TIMEOUT_MS     10000U
#define ESP_WIFI_TIMEOUT_MS      30000U

/* =========================================================
 * Global state
 * ========================================================= */

ESP8266_ConnectionState ESP_ConnState = ESP8266_DISCONNECTED;

static char esp_rx_buffer[ESP_RX_BUFFER_SIZE];

/* =========================================================
 * Logging: all messages go to Tera Term UART
 * ========================================================= */

static void ESP_Log(const char *fmt, ...)
{
    char log_buffer[ESP_LOG_BUFFER_SIZE];
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(log_buffer, sizeof(log_buffer) - 3U,
                      fmt, args);
    va_end(args);

    if (n < 0)
        return;

    size_t len = (size_t)n;

    if (len > sizeof(log_buffer) - 3U)
        len = sizeof(log_buffer) - 3U;

    log_buffer[len++] = '\r';
    log_buffer[len++] = '\n';
    log_buffer[len] = '\0';

    HAL_UART_Transmit(&TERATERM_UART,
                      (uint8_t *)log_buffer,
                      (uint16_t)len,
                      1000);
}

#define USER_LOG(...) ESP_Log(__VA_ARGS__)

#if ENABLE_DEBUG_LOG
#define DEBUG_LOG(...) ESP_Log(__VA_ARGS__)
#else
#define DEBUG_LOG(...) do { } while (0)
#endif

/* =========================================================
 * Internal UART helpers
 * ========================================================= */

/*
 * Discard bytes already waiting in the ESP UART receive path.
 * Call only when no command response needs to be preserved.
 */
static void ESP_FlushRx(void)
{
    uint8_t ch;

    while (HAL_UART_Receive(&ESP_UART, &ch, 1, 1) == HAL_OK)
    {
        /* Discard pending bytes */
    }

    memset(esp_rx_buffer, 0, sizeof(esp_rx_buffer));
}

/*
 * Read bytes until an expected substring is found, an explicit
 * error appears, the timeout expires, or the buffer fills.
 *
 * Returns ESP8266_OK only when the requested ACK is received.
 */
ESP8266_Status ESP_SendCommand(const char *cmd,
                               const char *ack,
                               uint32_t timeout)
{
    uint8_t ch;
    uint16_t idx = 0;
    uint32_t start;

    if (cmd == NULL || ack == NULL || ack[0] == '\0')
        return ESP8266_ERROR;

    /* Remove trailing bytes from a previous command. */
    ESP_FlushRx();

    DEBUG_LOG("TX: %s", cmd);

    if (HAL_UART_Transmit(&ESP_UART,
                          (uint8_t *)cmd,
                          (uint16_t)strlen(cmd),
                          HAL_MAX_DELAY) != HAL_OK)
    {
        USER_LOG("ESP UART transmit failed");
        return ESP8266_ERROR;
    }

    start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout &&
           idx < ESP_RX_BUFFER_SIZE - 1U)
    {
        if (HAL_UART_Receive(&ESP_UART, &ch, 1, 10) != HAL_OK)
            continue;

        esp_rx_buffer[idx++] = (char)ch;
        esp_rx_buffer[idx] = '\0';

        /*
         * Do not treat an ACK as successful if an explicit
         * command error has already appeared in the response.
         */
        if (strstr(esp_rx_buffer, "ERROR") != NULL ||
            strstr(esp_rx_buffer, "FAIL") != NULL)
        {
            DEBUG_LOG("ESP command error: %s", esp_rx_buffer);
            return ESP8266_ERROR;
        }

        if (strstr(esp_rx_buffer, "busy") != NULL)
        {
            DEBUG_LOG("ESP8266 is busy: %s", esp_rx_buffer);
            return ESP8266_ERROR;
        }

        if (strstr(esp_rx_buffer, ack) != NULL)
        {
            DEBUG_LOG("ACK matched: %s", ack);
            DEBUG_LOG("RX: %s", esp_rx_buffer);
            return ESP8266_OK;
        }
    }

    if (idx == 0U)
    {
        DEBUG_LOG("No response from ESP8266");
        return ESP8266_NO_RESPONSE;
    }

    DEBUG_LOG("Timeout waiting for [%s]. RX: %s",
              ack, esp_rx_buffer);

    return ESP8266_TIMEOUT;
}

/* =========================================================
 * ESP8266 initialization
 * ========================================================= */

ESP8266_Status ESP_Init(void)
{
    ESP8266_Status res;
    uint8_t ch;
    uint16_t idx;
    uint32_t start;
    uint8_t ready_found;

    USER_LOG("Initializing ESP8266...");
    HAL_Delay(1000);

    /* Step 1: Test basic communication, with retries. */
    res = ESP8266_NO_RESPONSE;

    for (int attempt = 1; attempt <= 3; attempt++)
    {
        res = ESP_SendCommand("AT\r\n", "OK", ESP_AT_TIMEOUT_MS);

        if (res == ESP8266_OK)
            break;

        USER_LOG("Initial AT attempt %d failed: %d",
                 attempt, res);

        HAL_Delay(500);
    }

    if (res != ESP8266_OK)
    {
        USER_LOG("ESP8266 is not responding to AT");
        return res;
    }

    USER_LOG("AT communication OK");

    /*
     * Step 2: Reset.
     *
     * Do not use ESP_SendCommand() here: the module can reboot
     * before it has finished transmitting its reset response.
     */
    USER_LOG("Resetting ESP8266...");

    ESP_FlushRx();

    {
        const char reset_cmd[] = "AT+RST\r\n";

        if (HAL_UART_Transmit(&ESP_UART,
                              (uint8_t *)reset_cmd,
                              sizeof(reset_cmd) - 1U,
                              1000) != HAL_OK)
        {
            USER_LOG("Failed to transmit AT+RST");
            return ESP8266_ERROR;
        }
    }

    /*
     * Wait for "ready", but do not assume every firmware emits
     * it in exactly the same way.
     */
    memset(esp_rx_buffer, 0, sizeof(esp_rx_buffer));
    idx = 0;
    ready_found = 0;
    start = HAL_GetTick();

    while ((HAL_GetTick() - start) < ESP_RESET_TIMEOUT_MS &&
           idx < ESP_RX_BUFFER_SIZE - 1U)
    {
        if (HAL_UART_Receive(&ESP_UART, &ch, 1, 10) == HAL_OK)
        {
            esp_rx_buffer[idx++] = (char)ch;
            esp_rx_buffer[idx] = '\0';

            if (strstr(esp_rx_buffer, "ready") != NULL)
            {
                ready_found = 1;
                break;
            }
        }
    }

    if (ready_found)
        USER_LOG("ESP8266 reset complete");
    else
        USER_LOG("No ready message; testing AT communication");

    /*
     * Allow boot output to settle. Then discard leftover reset
     * messages before sending a fresh command.
     */
    HAL_Delay(500);
    ESP_FlushRx();

    /* Step 3: Verify the module responds after reset. */
    res = ESP8266_NO_RESPONSE;

    for (int attempt = 1; attempt <= 3; attempt++)
    {
        res = ESP_SendCommand("AT\r\n", "OK", 3000);

        if (res == ESP8266_OK)
            break;

        USER_LOG("Post-reset AT attempt %d failed: %d",
                 attempt, res);

        HAL_Delay(500);
    }

    if (res != ESP8266_OK)
    {
        USER_LOG("ESP8266 did not respond after reset");
        return res;
    }

    USER_LOG("ESP8266 responding after reset");

    /* Step 4: Disable command echo. */
    res = ESP_SendCommand("ATE0\r\n", "OK", 5000);

    if (res != ESP8266_OK)
    {
        USER_LOG("ATE0 failed: %d", res);
        return res;
    }

    USER_LOG("ESP8266 initialized successfully");

    ESP_ConnState = ESP8266_DISCONNECTED;

    return ESP8266_OK;
}

/* =========================================================
 * Wi-Fi connection
 * ========================================================= */

ESP8266_Status ESP_ConnectWiFi(const char *ssid,
                               const char *password,
                               char *ip_buffer,
                               uint16_t buffer_len)
{
    ESP8266_Status result;
    char cmd[256];
    int n;

    if (ssid == NULL || password == NULL ||
        ip_buffer == NULL || buffer_len == 0U)
    {
        return ESP8266_ERROR;
    }

    ip_buffer[0] = '\0';
    ESP_ConnState = ESP8266_DISCONNECTED;

    USER_LOG("Setting ESP8266 station mode...");

    result = ESP_SendCommand("AT+CWMODE=1\r\n", "OK", 3000);

    if (result != ESP8266_OK)
    {
        USER_LOG("CWMODE failed: status=%d, RX=[%s]",
                 result, esp_rx_buffer);
        return result;
    }

    USER_LOG("Station mode configured");
    USER_LOG("Connecting to SSID: %s", ssid);

    n = snprintf(cmd, sizeof(cmd),
                 "AT+CWJAP=\"%s\",\"%s\"\r\n",
                 ssid, password);

    if (n < 0 || (size_t)n >= sizeof(cmd))
    {
        USER_LOG("Wi-Fi command exceeds buffer capacity");
        return ESP8266_ERROR;
    }

    /*
     * Wait for final OK rather than the intermediate
     * WIFI CONNECTED notification.
     */
    result = ESP_SendCommand(cmd, "OK", ESP_WIFI_TIMEOUT_MS);

    if (result != ESP8266_OK)
    {
        USER_LOG("Wi-Fi join failed: status=%d", result);
        USER_LOG("ESP response: %s", esp_rx_buffer);

        ESP_ConnState = ESP8266_NOT_CONNECTED;
        return result;
    }

    USER_LOG("Wi-Fi join succeeded");

    ESP_ConnState = ESP8266_CONNECTED_NO_IP;

    HAL_Delay(500);

    result = ESP_GetIP(ip_buffer, buffer_len);

    if (result != ESP8266_OK)
    {
        USER_LOG("Failed to retrieve IP: status=%d", result);
        ESP_ConnState = ESP8266_CONNECTED_NO_IP;
        return result;
    }

    USER_LOG("Wi-Fi connected; IP: %s", ip_buffer);

    return ESP8266_OK;
}

/* =========================================================
 * IP retrieval
 * ========================================================= */

ESP8266_Status ESP_GetIP(char *ip_buffer, uint16_t buffer_len)
{
    ESP8266_Status result;

    if (ip_buffer == NULL || buffer_len == 0U)
        return ESP8266_ERROR;

    ip_buffer[0] = '\0';

    USER_LOG("Fetching ESP8266 IP address...");

    for (int attempt = 1; attempt <= 3; attempt++)
    {
        result = ESP_SendCommand("AT+CIFSR\r\n", "OK", 5000);

        if (result != ESP8266_OK)
        {
            USER_LOG("CIFSR attempt %d failed: status=%d",
                     attempt, result);
            HAL_Delay(500);
            continue;
        }

        DEBUG_LOG("CIFSR response: %s", esp_rx_buffer);

        /*
         * Common response:
         * +CIFSR:STAIP,"192.168.1.100"
         * +CIFSR:STAMAC,"xx:xx:xx:xx:xx:xx"
         * OK
         */
        char *ip_start = strstr(esp_rx_buffer, "STAIP,\"");

        if (ip_start == NULL)
        {
            USER_LOG("STAIP not found on attempt %d", attempt);
            HAL_Delay(500);
            continue;
        }

        ip_start += strlen("STAIP,\"");

        char *ip_end = strchr(ip_start, '"');

        if (ip_end == NULL)
        {
            USER_LOG("Malformed STAIP response");
            HAL_Delay(500);
            continue;
        }

        size_t ip_len = (size_t)(ip_end - ip_start);

        if (ip_len == 0U || ip_len >= buffer_len)
        {
            USER_LOG("IP buffer too small or IP invalid");
            return ESP8266_ERROR;
        }

        memcpy(ip_buffer, ip_start, ip_len);
        ip_buffer[ip_len] = '\0';

        if (strcmp(ip_buffer, "0.0.0.0") == 0)
        {
            USER_LOG("IP not assigned yet; retrying");
            ip_buffer[0] = '\0';
            HAL_Delay(1000);
            continue;
        }

        ESP_ConnState = ESP8266_CONNECTED_IP;

        USER_LOG("IP address: %s", ip_buffer);

        return ESP8266_OK;
    }

    ip_buffer[0] = '\0';
    ESP_ConnState = ESP8266_CONNECTED_NO_IP;

    USER_LOG("Failed to obtain IP address");

    return ESP8266_ERROR;
}

/* =========================================================
 * Connection state
 * ========================================================= */

ESP8266_ConnectionState ESP_GetConnectionState(void)
{
    return ESP_ConnState;
}

/* =========================================================
 * HTTP response
 * ========================================================= */

void ESP_SendHttpResponse(uint8_t connectionId)
{
    static const char httpResponse[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 2\r\n"
        "Connection: close\r\n"
        "\r\n"
        "OK";

    char cmd[48];
    uint8_t byte;
    uint32_t start;
    uint8_t prompt_found = 0;

    int n = snprintf(cmd, sizeof(cmd),
                     "AT+CIPSEND=%u,%u\r\n",
                     (unsigned int)connectionId,
                     (unsigned int)(sizeof(httpResponse) - 1U));

    if (n < 0 || (size_t)n >= sizeof(cmd))
    {
        USER_LOG("CIPSEND command too long");
        return;
    }

    USER_LOG("Preparing HTTP response on connection %u",
             (unsigned int)connectionId);

    if (HAL_UART_Transmit(&ESP_UART,
                          (uint8_t *)cmd,
                          (uint16_t)strlen(cmd),
                          1000) != HAL_OK)
    {
        USER_LOG("Failed to send CIPSEND");
        return;
    }

    /* Wait for the ESP8266 send prompt. */
    start = HAL_GetTick();

    while ((HAL_GetTick() - start) < 3000U)
    {
        if (HAL_UART_Receive(&ESP_UART, &byte, 1, 10) == HAL_OK)
        {
            /* Forward received bytes to Tera Term for diagnosis. */
            HAL_UART_Transmit(&TERATERM_UART,
                              &byte, 1, 1000);

            if (byte == '>')
            {
                prompt_found = 1;
                break;
            }

            if (byte == 'F')
            {
                /* Continue reading; full errors are firmware-specific. */
            }
        }
    }

    if (!prompt_found)
    {
        USER_LOG("CIPSEND prompt timeout");
        return;
    }

    if (HAL_UART_Transmit(&ESP_UART,
                          (uint8_t *)httpResponse,
                          (uint16_t)(sizeof(httpResponse) - 1U),
                          3000) != HAL_OK)
    {
        USER_LOG("Failed to send HTTP response");
        return;
    }

    USER_LOG("HTTP response transmitted");
}
