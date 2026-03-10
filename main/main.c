#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Definiamo il numero del pin. Il LED integrato è quasi sempre sul 2.
#define LED_PIN 2

// Tag per i log (utile per il debugging)
static const char *TAG = "BLINK_EXAMPLE";

void app_main(void)
{
    ESP_LOGI(TAG, "Configurazione del LED sul pin %d...", LED_PIN);

    /* 1. RESET E DIREZIONE DEL PIN */
    // Ripristiniamo il pin allo stato iniziale
    gpio_reset_pin(LED_PIN);
    // Impostiamo il pin come USCITA (Output)
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    int stato = 0;

    while (1) {
        // Cambiamo lo stato del LED
        stato = !stato;
        
        /* 2. SCRITTURA SUL PIN */
        // Portiamo il pin a livello ALTO (1) o BASSO (0)
        gpio_set_level(LED_PIN, stato);

        // Stampiamo un messaggio nel monitor per vedere cosa succede
        if(stato) {
            ESP_LOGI(TAG, "LED ACCESO");
        } else {
            ESP_LOGI(TAG, "LED SPENTO");
        }

        /* 3. ATTESA (DELAY) */
        // vTaskDelay accetta i "tick" del sistema. 
        // Dividendo i millisecondi per portTICK_PERIOD_MS otteniamo il valore corretto.
        vTaskDelay(1000 / portTICK_PERIOD_MS); 
    }
}