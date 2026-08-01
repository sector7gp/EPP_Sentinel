#include "device_config.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "device_config";
static const char *NVS_NAMESPACE = "epp_cfg";

static device_config_t s_config;

static esp_err_t read_string(nvs_handle_t handle, const char *key, char *out, size_t out_size)
{
    size_t len = out_size;
    esp_err_t err = nvs_get_str(handle, key, out, &len);
    if (err != ESP_OK) {
        out[0] = '\0';
    }
    return err;
}

esp_err_t device_config_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    memset(&s_config, 0, sizeof(s_config));

    nvs_handle_t handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Sin configuración previa en NVS (%s)", esp_err_to_name(err));
        return ESP_OK;
    }

    read_string(handle, "backend_url", s_config.backend_url, sizeof(s_config.backend_url));
    read_string(handle, "device_id", s_config.device_id, sizeof(s_config.device_id));
    read_string(handle, "device_token", s_config.device_token, sizeof(s_config.device_token));

    uint8_t provisioned = 0;
    nvs_get_u8(handle, "provisioned", &provisioned);
    s_config.provisioned = provisioned && s_config.backend_url[0] && s_config.device_id[0] &&
                            s_config.device_token[0];

    nvs_close(handle);

    ESP_LOGI(TAG, "Configuración cargada: provisioned=%d device_id=%s",
              s_config.provisioned, s_config.provisioned ? s_config.device_id : "(-)");
    return ESP_OK;
}

bool device_config_is_provisioned(void)
{
    return s_config.provisioned;
}

const device_config_t *device_config_get(void)
{
    return &s_config;
}

esp_err_t device_config_save(const char *backend_url, const char *device_id,
                              const char *device_token)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open falló: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, "backend_url", backend_url);
    err = err ?: nvs_set_str(handle, "device_id", device_id);
    err = err ?: nvs_set_str(handle, "device_token", device_token);
    err = err ?: nvs_set_u8(handle, "provisioned", 1);
    err = err ?: nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo guardar la configuración: %s", esp_err_to_name(err));
        return err;
    }

    strlcpy(s_config.backend_url, backend_url, sizeof(s_config.backend_url));
    strlcpy(s_config.device_id, device_id, sizeof(s_config.device_id));
    strlcpy(s_config.device_token, device_token, sizeof(s_config.device_token));
    s_config.provisioned = true;

    ESP_LOGI(TAG, "Configuración guardada para device_id=%s", device_id);
    return ESP_OK;
}
