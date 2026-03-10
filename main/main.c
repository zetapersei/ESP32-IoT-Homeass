#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"

// Configurazione Wi-Fi
#define EXAMPLE_ESP_WIFI_SSID      "Mio_ESP32_AP"
#define EXAMPLE_ESP_WIFI_PASS      "password123"
#define EXAMPLE_MAX_STA_CONN       4

static const char *TAG = "WIFI_SERVER";

/* --- 1. GESTORE DELLA PAGINA WEB --- */
// Questa funzione viene chiamata quando qualcuno visita la pagina principale "/"
esp_err_t get_handler(httpd_req_t *req) {
    const char* resp_str = "<h1>Benvenuto sul server ESP32!</h1><p>Il server funziona correttamente.</p>";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

// Definiamo l'URI (l'indirizzo) e il metodo (GET) per la pagina
httpd_uri_t uri_get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

/* --- 2. AVVIO DEL SERVER HTTP --- */
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_get);
        ESP_LOGI(TAG, "Server HTTP avviato!");
    }
    return server;
}

/* --- 3. CONFIGURAZIONE WI-FI AP --- */
void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP Pronto. SSID: %s Password: %s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

void app_main(void) {
    // Inizializzazione NVS (necessaria per salvare impostazioni Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Avvio Wi-Fi e Server
    wifi_init_softap();
    start_webserver();
}