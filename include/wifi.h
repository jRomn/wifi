/**
 * @file wifi.h
 * @brief WiFi Controller Initialization — connects ESP32 to an existing network.
 *
 * Credentials are configured via `idf.py menuconfig` under "WiFi Configuration".
 * - SSID: Your WiFi network name (SSID)
 * - Password: Your WiFi network password
 * Call `wifi_init_sta()` once at startup before starting any network-dependent
 * components. The function blocks until an IP address is assigned or the retry
 * limit is exhausted.
 */

#pragma once

// =============================
// Header Files 
// =============================

    /* --- General --- */
    #include "esp_err.h"

// =============================
// Application Log Tag
// =============================
   
    #define WiFi_TAG "WiFi"   // ESP_LOGI (Info)
    
// =============================
// Main Functions:
// =============================

    /**
    * @brief Initialize WiFi in station (STA) mode and connect to the configured AP.
    * 
    * Note: WiFi Modes: 
    * - STA stands for "station" — the IEEE 802.11 standard's term for a client device on a wireless network.
    * - AP stands for "Access Point" — the device (like a router) that provides WiFi connectivity to clients.
    * - APSTA stands for "Access Point + Station" mode, where the ESP32 can act as both a client and an access point simultaneously.
    *
    * Internally initializes the TCP/IP stack, registers event handlers, and blocks
    * on an EventGroup until the IP_EVENT_STA_GOT_IP event is received.
    *
    * @return ESP_OK on successful connection and IP assignment.
    * @return ESP_FAIL if the maximum retry count is exhausted without success.
    */

    /* ---> STEP 3.0 : Initialize WiFi Station -> wifi.c */
    esp_err_t wifi_init_start(void);

