// =============================
// Header Files 
// =============================

    /* --- General --- */
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_log.h"
    #include "esp_err.h"

    /* --- WiFi --- */
    #include "wifi.h"
    #include "esp_wifi.h"
    #include "esp_event.h"
    #include "esp_netif.h"
    #include "nvs_flash.h"
    #include "freertos/event_groups.h"

// =============================
// WiFi Configuration (Exposed for wifi.c)
// =============================
/* EventGroup bits used to signal connection outcome to the blocking caller. */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_retry_count = 0;

/* -------------------------------------------------------------------------
 * Event Handler
 * Registered for both WIFI_EVENT and IP_EVENT on the default event loop.
 * Sets EventGroup bits to unblock wifi_init_sta() when the outcome is known.
 * ------------------------------------------------------------------------- */
static void event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < CONFIG_WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGW(WIFI_TAG, "Retrying connection (%d/%d)...",
                     s_retry_count, CONFIG_WIFI_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(WIFI_TAG, "Connection failed after %d retries.", CONFIG_WIFI_MAXIMUM_RETRY);
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(WIFI_TAG, "Connected. IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


// =============================
// Main Functions:
// =============================

    /* ---> STEP 3.0 : Initialize WiFi Station */

    // =============================
    // WiFi Unit Initialization + Channel Configuration + Calibration
    // =============================
    // Function to initialize the WiFi unit, configure the channel, and set up calibration.
    // Returns a handle to the initialized WiFi unit.
    esp_err_t wifi_init_sta(void){
        
        /* ---> PRE-STEP : Non-Volatile Storage ( NVS ) setup for Radio Calibration & Credentials */
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        /* ---> Create Async Signaling Register (Event Group) */
        s_wifi_event_group = xEventGroupCreate();

        /* ---> Load the TCP/IP Network Stack (lwIP) */
        ESP_ERROR_CHECK(esp_netif_init());

        /* ---> Start the Event Dispatcher (Event Loop) */
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        
        /* ---> Create a Logical Station Interface (bridge between TCP/IP and radio) */
        esp_netif_create_default_wifi_sta();

        /* ---> Define the Controller Blueprint (golden settings) */
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

        /* ---> Initialize the WiFi Controller (allocate driver + program hardware registers) */
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        /* ---> STEP 3A : Register the WiFi Radio Events Listener (All Types) */
        esp_event_handler_instance_t instance_any_id;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT,               // Department: WiFi Radio
            ESP_EVENT_ANY_ID,         // Subject: "Tell me about every WiFi event"
            &event_handler,           // Map all these events to this callback function
            NULL,
            &instance_any_id));       // Receipt: Keep this handle for later unsubscription

        /* ---> STEP 3B : Register the IP Stack Events Listener (Specific Event) */
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT,                 // Department: TCP/IP Network Stack
            IP_EVENT_STA_GOT_IP,      // Subject: "I just received an IP address"
            &event_handler,           // Map this specific event to the same callback function
            NULL,
            &instance_got_ip));       // Receipt: Keep this handle for later unsubscription


        /* ---> STEP 4 : Build the Station Configuration (SSID & Password) */
        wifi_config_t wifi_config = {
            .sta = {
                .ssid     = CONFIG_WIFI_SSID,
                .password = CONFIG_WIFI_PASSWORD,
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
        };

        /* ---> STEP 5A : Set the Operating Mode (Station = Client) */
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

        /* ---> STEP 5B : Deliver the Credentials to the Driver */
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

        /* ---> STEP 5C : Power On the Radio */
        ESP_ERROR_CHECK(esp_wifi_start());

        /* ---> STEP 6 : Wait for Connection Result */
        /* Block inside wifi_init_sta() until the event handler signals success or failure */
        EventBits_t bits = xEventGroupWaitBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,   /* Do not clear bits on exit */
            pdFALSE,   /* Wait for either bit, not both */
            portMAX_DELAY);

        /* ---> Clean Up (no longer needed after connection) */
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
            WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));

        vEventGroupDelete(s_wifi_event_group);

        /* Final result */
        if (bits & WIFI_CONNECTED_BIT) {
            return ESP_OK;        // Successfully connected and received IP address
        }

        return ESP_FAIL;          // Connection failed
    }