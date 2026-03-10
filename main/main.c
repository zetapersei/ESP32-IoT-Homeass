#include <stdio.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

void app_main(void) {
    // 1. Inizializzazione della partizione NVS predefinita
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Se la memoria è piena o corrotta, la cancelliamo e riproviamo
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // 2. Apertura di un "Namespace" chiamato "storage" in modalità lettura/scrittura
    nvs_handle_t my_handle;
    printf("\nApertura NVS...");
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    
    if (err != ESP_OK) {
        printf("Errore nell'apertura NVS (%s)\n", esp_err_to_name(err));
    } else {
        printf("Fatto!\n");

        // 3. Lettura del valore
        int32_t restart_counter = 0; // valore di default se non trovato
        err = nvs_get_i32(my_handle, "restart_con", &restart_counter);
        
        switch (err) {
            case ESP_OK:
                printf("Valore letto: %d\n", (int)restart_counter);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("Valore non trovato, inizializzazione...\n");
                break;
            default:
                printf("Errore nella lettura (%s)\n", esp_err_to_name(err));
        }

        // 4. Incremento e Scrittura
        restart_counter++;
        printf("Aggiornamento contatore a: %d\n", (int)restart_counter);
        err = nvs_set_i32(my_handle, "restart_con", restart_counter);
        printf((err != ESP_OK) ? "Scrittura fallita!\n" : "Scrittura eseguita.\n");

        // 5. COMMIT: Importante! Salva effettivamente i dati sulla Flash
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Commit fallito!\n" : "Dati salvati permanentemente.\n");

        // 6. Chiusura del handle
        nvs_close(my_handle);
    }
}