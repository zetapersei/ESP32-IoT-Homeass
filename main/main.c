#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

#define LED_PIN 25
#define WIFI_SSID "ESP32_Memoria_LED"
#define WIFI_PASS "password123"

static const char *TAG = "WEB_NVS_APP";

// --- FUNZIONE PER SALVARE LO STATO NELLA NVS ---
void save_led_state(int8_t state) {
    nvs_handle_t my_handle;
    if (nvs_open("storage", NVS_READWRITE, &my_handle) == ESP_OK) {
        nvs_set_i8(my_handle, "led_state", state);
        nvs_commit(my_handle);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "Stato LED salvato: %d", state);
    }
}

// --- FUNZIONE PER LEGGERE LO STATO DALLA NVS ---
int8_t get_led_state() {
    nvs_handle_t my_handle;
    int8_t state = 0; // Default: spento
    if (nvs_open("storage", NVS_READONLY, &my_handle) == ESP_OK) {
        nvs_get_i8(my_handle, "led_state", &state);
        nvs_close(my_handle);
    }
    return state;
}

/* GESTORE PAGINA PRINCIPALE */
esp_err_t index_handler(httpd_req_t *req) {
    int8_t current_state = get_led_state();
    char resp_str[600];
    
    // Creiamo l'HTML dinamicamente per mostrare lo stato attuale
    snprintf(resp_str, sizeof(resp_str),
             "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
             "<style>body{font-family:Arial;text-align:center;padding:20px;} "
             ".btn{display:block;padding:20px;margin:10px;color:white;text-decoration:none;font-size:20px;border-radius:10px;} "
             ".on{background-color:green;} .off{background-color:red;} "
             ".status{font-size:24px; font-weight:bold; color:%s;}</style></head>"
             "<body><h1>Controllo Resiliente</h1>"
             "<p>Stato attuale: <span class='status'>%s</span></p>"
             "<a href='/on' class='btn on'>ACCENDI</a>"
             "<a href='/off' class='btn off'>SPEGNI</a></body></html>",
             current_state ? "green" : "red",
             current_state ? "ACCESO" : "SPENTO");

    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

/* AZIONE ACCENDI */
esp_err_t on_handler(httpd_req_t *req) {
    gpio_set_level(LED_PIN, 1);
    save_led_state(1); // SALVA NELLA FLASH
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* AZIONE SPEGNI */
esp_err_t off_handler(httpd_req_t *req) {
    gpio_set_level(LED_PIN, 0);
    save_led_state(0); // SALVA NELLA FLASH
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// --- AVVIO SERVER E WIFI (Invariati) ---
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_idx = { .uri="/", .method=HTTP_GET, .handler=index_handler };
        httpd_uri_t uri_on  = { .uri="/on", .method=HTTP_GET, .handler=on_handler };
        httpd_uri_t uri_off = { .uri="/off", .method=HTTP_GET, .handler=off_handler };
        httpd_register_uri_handler(server, &uri_idx);
        httpd_register_uri_handler(server, &uri_on);
        httpd_register_uri_handler(server, &uri_off);
    }
    return server;
}

void wifi_init_softap(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = { .ap = { .ssid=WIFI_SSID, .password=WIFI_PASS, .max_connection=4, .authmode=WIFI_AUTH_WPA_WPA2_PSK } };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
}

void app_main(void) {
    // 1. Inizializza NVS (Obbligatorio per leggere/scrivere)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Configura LED
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    // 3. RECUPERA STATO PRECEDENTE E APPLICALO
    int8_t last_state = get_led_state();
    gpio_set_level(LED_PIN, last_state);
    ESP_LOGI(TAG, "Ripristinato stato LED: %d", last_state);

    // 4. Avvia Wi-Fi e Server
    wifi_init_softap();
    start_webserver();
}