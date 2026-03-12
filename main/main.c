#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "esp_timer.h"
#include "rom/ets_sys.h" // Per ets_delay_us
#include "esp_mac.h"
#include "esp_modem_config.h"
#include "esp_modem_api.h"
#include "esp_netif_ppp.h"
#include "esp_sleep.h"



// --- CONFIGURAZIONE ---

#define BROKER_URL     "mqtt://picogalliiotserver.ddns.net:1883" // IP Mosquitto Pubblico
#define APN            "internet.it" // Cambia con APN della tua SIM

// --- VARIABILI NELLA MEMORIA RTC ---
RTC_DATA_ATTR float last_temp = -100.0;
RTC_DATA_ATTR float last_hum = -100.0;



#define TEMP_THRESHOLD 0.5  // Invia solo se cambia di 0.5 gradi
#define MAX_SLEEP_SEC 3600  // Massimo 1 ora di sonno

#define DHT_GPIO 4
#define MODEM_PWRKEY_PIN 13

// Configurazione UART (GPIO 17 TX, 16 RX)
#define UART_NUM UART_NUM_2

static const char *TAG = "IOT_NODE";


static esp_mqtt_client_handle_t client;
static esp_modem_dce_t *dce; 

static esp_err_t read_dht_raw(uint8_t data[5]) {
    uint8_t last_state = 1;
    uint8_t counter = 0;
    uint8_t j = 0, i;

    data[0] = data[1] = data[2] = data[3] = data[4] = 0;

    // FASE 1: Segnale di Start
    gpio_set_direction(DHT_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(20)); // Almeno 18ms
    gpio_set_level(DHT_GPIO, 1);
    ets_delay_us(40);
    gpio_set_direction(DHT_GPIO, GPIO_MODE_INPUT);

    // FASE 2: Lettura dei 40 bit (Risposta del sensore)
    for (i = 0; i < 85; i++) {
        counter = 0;
        while (gpio_get_level(DHT_GPIO) == last_state) {
            counter++;
            ets_delay_us(1);
            if (counter == 255) break;
        }
        last_state = gpio_get_level(DHT_GPIO);
        if (counter == 255) break;

        if ((i >= 4) && (i % 2 == 0)) {
            data[j / 8] <<= 1;
            if (counter > 28) data[j / 8] |= 1; // Un impulso lungo significa '1'
            j++;
        }
    }

    // FASE 3: Verifica Checksum
    if ((j >= 40) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))) return ESP_OK;
    return ESP_FAIL;
}

void modem_power_down() {
    const char *cmd = "AT+CPOWD=1\r\n";
    ESP_LOGI(TAG, "Spegnimento modem SIM800L...");
    
    // Inviamo il comando direttamente sulla UART del modem
    uart_write_bytes(UART_NUM_2, cmd, strlen(cmd));
    
    // Attendiamo che il modem risponda o si spenga (solitamente 2-3 secondi)
    vTaskDelay(pdMS_TO_TICKS(3000));
}

void modem_power_on() {
    ESP_LOGI(TAG, "Inizializzazione accensione modem...");
    
    // Configura il pin PWRKEY
    gpio_reset_pin(MODEM_PWRKEY_PIN);
    gpio_set_direction(MODEM_PWRKEY_PIN, GPIO_MODE_OUTPUT);

    // Sequenza di accensione: 
    // 1. Tieni alto (IDLE)
    gpio_set_level(MODEM_PWRKEY_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 2. Impulso LOW (per il SIM800L servono almeno 1.2 - 2 secondi)
    ESP_LOGI(TAG, "Pulsante PWRKEY premuto...");
    gpio_set_level(MODEM_PWRKEY_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(2000)); 
    
    // 3. Rilascia (TORNA ALTO)
    gpio_set_level(MODEM_PWRKEY_PIN, 1);
    ESP_LOGI(TAG, "Pulsante PWRKEY rilasciato. Modem in avvio.");
    
    // 4. Attendi che il modem si registri o sia pronto per i comandi AT
    vTaskDelay(pdMS_TO_TICKS(3000));
}



void telemetry_task(void *pvParameters) {
    uint8_t data[5];
    uint8_t mac[6];
    int rssi = 0, ber = 0; // ber è il bit error rate, di solito meno importante
    char json_string[180];
    
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // 1. Leggi la qualità del segnale dal modem
    // dce è l'oggetto creato in app_main (assicurati che sia accessibile o globale)
    if (esp_modem_get_signal_quality(dce, &rssi, &ber) == ESP_OK) {
        ESP_LOGI(TAG, "Qualità segnale GSM: RSSI=%d", rssi);
    }

    if (read_dht_raw(data) == ESP_OK) {
        float h = (float)data[0];
        float t = (float)data[2];

        float diff_t = (t > last_temp) ? (t - last_temp) : (last_temp - t);
        
        if (diff_t >= 0.5 || last_temp == -100.0) {
            last_temp = t;
            
            // Inseriamo l'RSSI nel JSON
            snprintf(json_string, sizeof(json_string), 
                     "{\"id\":\"ESP32_%02X%02X\", \"t\":%.1f, \"h\":%.1f, \"rssi\":%d}", 
                     mac[4], mac[5], t, h, rssi);

            if (client != NULL) {
                esp_mqtt_client_publish(client, "/casa/sensori", json_string, 0, 1, 0);
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }
        
        // Procedura di spegnimento e Deep Sleep
        esp_mqtt_client_stop(client);
        modem_power_down();
        esp_sleep_enable_timer_wakeup(900 * 1000000); 
        esp_deep_sleep_start();
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

// --- GESTORE EVENTI DI RETE  ---
static void network_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == IP_EVENT && event_id == IP_EVENT_PPP_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connesso a GPRS! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        esp_mqtt_client_start(client);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGW(TAG, "Connessione GPRS persa");
        esp_mqtt_client_stop(client);
    }
}





void app_main(void) {
    // 1. Inizializzazione base (NVS e Loop eventi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    modem_power_on();

    // 2. Configurazione dell'interfaccia di rete PPP
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);
    assert(esp_netif);

    // Inizializzazione del LED/Relè sul GPIO 25
    gpio_reset_pin(GPIO_NUM_25);
    gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_25, 0); // Inizia spento

    // 3. Configurazione del Modem (DTE e DCE)
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.port_num = UART_NUM_2;
    dte_config.uart_config.tx_io_num = 17;
    dte_config.uart_config.rx_io_num = 16;
    dte_config.uart_config.baud_rate = 115200;
    
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(APN); // Cambia con APN della tua SIM

    // Creazione dell'oggetto modem per SIM800
    dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM800, &dte_config, &dce_config, esp_netif);
    assert(dce);

    // 4. Registrazione handler per l'IP via PPP
    // Quando il modem ottiene l'IP dalla rete cellulare, scatta IP_EVENT_PPP_GOT_IP
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, &network_event_handler, NULL, NULL));

    // 5. Configurazione MQTT (stessa di prima, ma con timeout più lunghi)
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_URL,
        .network.reconnect_timeout_ms = 10000, // Più tempo per il GPRS
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    // 6. Avvio connessione modem
    ESP_LOGI(TAG, "Connessione GPRS in corso...");
    // Il modem inizierà la negoziazione PPP e chiamerà l'evento GOT_IP
}