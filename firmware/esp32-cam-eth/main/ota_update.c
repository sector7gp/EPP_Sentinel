#include "ota_update.h"

#include "esp_log.h"
#include "esp_ota_ops.h"

static const char *TAG = "ota_update";

static esp_ota_handle_t s_handle;
static const esp_partition_t *s_target;
static bool s_in_progress;

esp_err_t ota_update_begin(void)
{
    if (s_in_progress) {
        ESP_LOGW(TAG, "Ya hay una actualización en curso");
        return ESP_ERR_INVALID_STATE;
    }

    s_target = esp_ota_get_next_update_partition(NULL);
    if (!s_target) {
        ESP_LOGE(TAG, "No se encontró partición OTA destino");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Escribiendo OTA en '%s' (offset 0x%lx, %lu bytes)", s_target->label,
              (unsigned long)s_target->address, (unsigned long)s_target->size);

    esp_err_t err = esp_ota_begin(s_target, OTA_SIZE_UNKNOWN, &s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin falló: %s", esp_err_to_name(err));
        return err;
    }
    s_in_progress = true;
    return ESP_OK;
}

esp_err_t ota_update_write(const uint8_t *data, size_t len)
{
    if (!s_in_progress) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_ota_write(s_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write falló: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ota_update_finish(void)
{
    if (!s_in_progress) {
        return ESP_ERR_INVALID_STATE;
    }
    s_in_progress = false;

    esp_err_t err = esp_ota_end(s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end falló (imagen inválida o incompleta): %s", esp_err_to_name(err));
        return err;
    }
    err = esp_ota_set_boot_partition(s_target);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition falló: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "OTA completa, arrancará en '%s' tras reiniciar", s_target->label);
    return ESP_OK;
}

void ota_update_abort(void)
{
    if (!s_in_progress) {
        return;
    }
    s_in_progress = false;
    esp_ota_abort(s_handle);
    ESP_LOGW(TAG, "OTA abortada");
}
