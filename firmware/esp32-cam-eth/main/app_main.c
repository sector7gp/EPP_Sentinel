#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "backend_client.h"
#include "camera.h"
#include "device_config.h"
#include "net_eth.h"
#include "provisioning_server.h"
#include "scheduler.h"
#include "status_led.h"
#include "time_sync.h"

static const char *TAG = "app_main";

#define CONFIG_POLL_INTERVAL_SEC 300
#define LOOP_TICK_MS 2000
// Cubre con margen la peor secuencia de llamadas bloqueantes de una
// iteración (fetch config ~15s + reintento + captura+subida ~30s).
#define TASK_WDT_TIMEOUT_MS 90000

static void wait_forever(void)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    status_led_init();
    status_led_set(STATUS_LED_BOOTING);

    ESP_ERROR_CHECK(device_config_init());

    if (net_eth_start(20000) != ESP_OK) {
        ESP_LOGW(TAG, "Sin IP por Ethernet al arrancar; se seguirá reintentando en segundo plano");
    }

    provisioning_server_start();

    if (!device_config_is_provisioned()) {
        ESP_LOGW(TAG, "Dispositivo sin provisionar. Abra el portal web (ver README del "
                       "firmware) para cargar backend_url/device_id/device_token; el equipo "
                       "se reiniciará solo al guardar.");
        status_led_set(STATUS_LED_ERROR);
        wait_forever();
    }

    time_sync_start(15000);

    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo inicializar la cámara OV5640; revise el pinout en menuconfig");
        provisioning_status_set_note("Error: no se pudo inicializar la cámara (revisar pinout)");
        status_led_set(STATUS_LED_ERROR);
        wait_forever();
    }

    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = TASK_WDT_TIMEOUT_MS,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_task_wdt_reconfigure(&wdt_config);
    esp_task_wdt_add(NULL);

    ESP_LOGI(TAG, "Agente iniciado para dispositivo %s", device_config_get()->device_id);
    status_led_set(STATUS_LED_OK);

    remote_config_t remote_config = {0};
    bool have_config = false;
    char last_config_version[64] = "";
    time_t last_config_fetch = 0;
    time_t last_capture = 0;
    int upload_retries = 0;
    time_t next_retry_at = 0;

    // Único frame pendiente en RAM (sin cola persistente multi-item, ver
    // decisión de diseño en el plan): un intervalo nuevo reemplaza al pendiente.
    uint8_t *pending_copy = NULL;
    size_t pending_len = 0;

    while (1) {
        esp_task_wdt_reset();
        time_t now = time(NULL);

        if (last_config_fetch == 0 || now - last_config_fetch >= CONFIG_POLL_INTERVAL_SEC) {
            remote_config_t fetched;
            if (backend_client_fetch_config(&fetched) == ESP_OK) {
                if (strcmp(fetched.config_version, last_config_version) != 0) {
                    ESP_LOGI(TAG, "Configuración actualizada: %s", fetched.config_version);
                    strlcpy(last_config_version, fetched.config_version, sizeof(last_config_version));
                }
                remote_config = fetched;
                have_config = true;
            } else {
                ESP_LOGW(TAG, "No se pudo obtener configuración remota");
            }
            last_config_fetch = now;
        }

        if (pending_copy && now >= next_retry_at) {
            if (backend_client_upload(pending_copy, pending_len, remote_config.stream_id) == ESP_OK) {
                ESP_LOGI(TAG, "Subida (reintento) exitosa");
                provisioning_status_set_capture(true, "reintento OK");
                free(pending_copy);
                pending_copy = NULL;
                pending_len = 0;
                upload_retries = 0;
            } else {
                upload_retries++;
                next_retry_at = now + backend_client_retry_delay_seconds(upload_retries);
                ESP_LOGW(TAG, "Reintento de subida falló (retries=%d, próximo en %ds)",
                          upload_retries, backend_client_retry_delay_seconds(upload_retries));
                provisioning_status_set_capture(false, "reintentando subida");
            }
        }

        if (have_config && time_sync_is_synced() &&
            scheduler_is_within_schedule(&remote_config.schedule, now)) {
            int interval = scheduler_interval_seconds(&remote_config.schedule);
            if (now - last_capture >= interval) {
                last_capture = now;
                const uint8_t *buf = NULL;
                size_t len = 0;
                if (camera_capture_jpeg(remote_config.image_width, remote_config.image_height,
                                          remote_config.image_jpeg_quality,
                                          remote_config.image_max_kb, &buf, &len) == ESP_OK) {
                    if (backend_client_upload(buf, len, remote_config.stream_id) == ESP_OK) {
                        ESP_LOGI(TAG, "Captura subida (%u bytes)", (unsigned)len);
                        provisioning_status_set_capture(true, "OK");
                        if (pending_copy) {
                            free(pending_copy);
                            pending_copy = NULL;
                            pending_len = 0;
                        }
                        upload_retries = 0;
                    } else {
                        ESP_LOGW(TAG, "Fallo al subir la captura; se reintentará con backoff");
                        if (pending_copy) {
                            free(pending_copy);
                        }
                        pending_copy = malloc(len);
                        if (pending_copy) {
                            memcpy(pending_copy, buf, len);
                            pending_len = len;
                        }
                        upload_retries = 1;
                        next_retry_at = now + backend_client_retry_delay_seconds(upload_retries);
                        provisioning_status_set_capture(false, "subida falló, reintentando");
                    }
                    camera_release();
                } else {
                    ESP_LOGE(TAG, "Fallo de captura de cámara");
                    provisioning_status_set_note("Error de captura de cámara");
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(LOOP_TICK_MS));
    }
}
