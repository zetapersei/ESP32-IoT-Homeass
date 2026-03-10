#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "esp_timer.h"

static const char *TAG = "IOT_NODE";

// --- CONFIGURAZIONE ---
#define WIFI_SSID      "Duca_Home"
#define WIFI_PASS      "4655407195808313"
#define BROKER_URL     "mqtt://picogalliiotserver.ddns.net:1883" // IP Mosquitto Pubblico


static esp_mqtt_client_handle_t client;



void telemetry_task(void *pvParameters) {
    int counter = 0;
    char data_str[20];

    while (1) {
        // Prepariamo una stringa con un valore (es. un contatore o sensore)
        snprintf(data_str, sizeof(data_str), "%d", counter++);
        
        // Inviamo al broker (Topic, Messaggio, Lunghezza (0=auto), QoS, Retain)
        esp_mqtt_client_publish(client, "/dispositivo/stato", data_str, 0, 1, 0);
        
        ESP_LOGI(TAG, "Dati inviati: %s", data_str);
        
        // Aspetta 10 secondi
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// --- GESTORE EVENTI MQTT ---
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connesso!");
            // Ci iscriviamo al topic dei comandi
            esp_mqtt_client_subscribe(client, "/casa/led", 0);
    
            // Avviamo l'invio dei dati in un thread separato (se non è già attivo)
            static bool task_started = false;
            if (!task_started) {
                xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);
                task_started = true;
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Disconnesso. Tentativo riconnessione...");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Errore MQTT!");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Messaggio ricevuto sul topic: %.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "Contenuto: %.*s", event->data_len, event->data);

            // Esempio: Se ricevo "ON" sul topic /casa/led, accendo il GPIO 2
            if (strncmp(event->data, "ON", event->data_len) == 0) {
                gpio_set_level(GPIO_NUM_25, 1);
                ESP_LOGI(TAG, "LED ACCESO");
            } else if (strncmp(event->data, "OFF", event->data_len) == 0) {
                gpio_set_level(GPIO_NUM_25, 0);
                ESP_LOGI(TAG, "LED SPENTO");
            }
            break;
        default: break;
    }
}

// --- GESTORE EVENTI DI RETE (Wi-Fi & IP) ---
static void network_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi Avviato. Connessione in corso...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi perso! Riprovo tra 5 secondi...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP Ottenuto: " IPSTR, IP2STR(&event->ip_info.ip));
        
        // AVVIO MQTT SOLO DOPO AVER RICEVUTO L'IP
        esp_mqtt_client_start(client);
    }
}



void app_main(void) {
    // 1. Inizializzazione NVS (Fondamentale per il Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1. Inizializza il pin (es. il LED  è sul GPIO 25)
    gpio_reset_pin(GPIO_NUM_25);
    // 2. Impostalo come uscita
    gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);

    // 2. Inizializzazione Rete
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3. Registrazione Handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &network_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &network_event_handler, NULL, NULL));

    // 4. Configurazione Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // Più compatibile
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 5. Configurazione MQTT (Preparazione, non avvio)
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_URL,
        .task.stack_size = 8192,
        .credentials = {
            .username = "maurizio",
            .authentication.password = "Ct66ubw82m",
        },
        .network.reconnect_timeout_ms = 5000, // Riprova ogni 5 secondi se cade la linea
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    // 6. Start!
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Disabilita Power Save per evitare latenze fastidiose
    esp_wifi_set_ps(WIFI_PS_NONE);
}