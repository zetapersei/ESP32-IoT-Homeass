#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "MQTT_EXAMPLE";

/* --- 1. GESTORE DEGLI EVENTI MQTT --- */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            // Appena connessi, ci iscriviamo a un topic
            msg_id = esp_mqtt_client_subscribe(client, "/topic/esp32_test", 0);
            ESP_LOGI(TAG, "Sottoscritto con successo, msg_id=%d", msg_id);
            
            // Inviamo un messaggio di saluto
            esp_mqtt_client_publish(client, "/topic/esp32_test", "Ciao dal mio ESP32!", 0, 1, 0);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Messaggio ricevuto!");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            break;
    }
}

/* --- 2. CONFIGURAZIONE E AVVIO MQTT --- */
static void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://picogalliiotserver.ddns.net:1883", // Sostituisci con l'IP o URL
        .credentials = {
            .username = "maurizio",         // Inserisci lo username del broker
            .authentication.password = "Ct66ubw82m", // Inserisci la password
            .client_id = "ESP32"
        },
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    
    /* Registra l'handler degli eventi */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    /* Avvia il client */
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "Client MQTT inizializzato con autenticazione.");
}

/* --- 3. WIFI STATION (Inserisci i tuoi dati qui) --- */
void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Duca_Home",
            .password = "4655407195808313",
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    
    ESP_LOGI(TAG, "Connessione Wi-Fi in corso...");
    wifi_init_sta();
    
    // Attendiamo un po' che il Wi-Fi si colleghi prima di avviare MQTT
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    
    mqtt_app_start();
}