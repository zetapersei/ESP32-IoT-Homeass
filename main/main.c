#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

#define LED_PIN 0
#define EXAMPLE_ESP_WIFI_SSID      "ESP32_Server_LED"
#define EXAMPLE_ESP_WIFI_PASS      "password123"

static const char *TAG = "WEB_SERVER";

// 1. Pagina HTML inviata al browser
esp_err_t index_handler(httpd_req_t *req) {
    const char* resp_str = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
                           "<style>body{font-family:Arial;text-align:center;} .btn{display:block;padding:20px;margin:10px;color:white;text-decoration:none;font-size:20px;border-radius:10px;} "
                           ".on{background-color:green;} .off{background-color:red;}</style></head>"
                           "<body><h1>ESP32 Controllo</h1>"
                           "<a href='/on' class='btn on'>ACCENDI</a>"
                           "<a href='/off' class='btn off'>SPEGNI</a></body></html>";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

// 2. Azione Accendi
esp_err_t on_handler(httpd_req_t *req) {
    gpio_set_level(LED_PIN, 1);
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// 3. Azione Spegni
esp_err_t off_handler(httpd_req_t *req) {
    gpio_set_level(LED_PIN, 0);
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// 4. Avvio del Server
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

// 5. Configurazione Wi-Fi
void wifi_init_softap(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = {
        .ap = { .ssid=EXAMPLE_ESP_WIFI_SSID, .password=EXAMPLE_ESP_WIFI_PASS, .max_connection=4, .authmode=WIFI_AUTH_WPA_WPA2_PSK },
    };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
}

void app_main(void) {
    nvs_flash_init();
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    wifi_init_softap();
    start_webserver();
}