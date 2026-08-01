#include "time_sync.h"

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "sdkconfig.h"

static const char *TAG = "time_sync";

esp_err_t time_sync_start(uint32_t timeout_ms)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_EPP_NTP_SERVER);
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_sntp_init falló: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeout_ms));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo sincronizar hora con %s (%s); se reintentará en segundo plano",
                  CONFIG_EPP_NTP_SERVER, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Hora sincronizada con %s", CONFIG_EPP_NTP_SERVER);
    return ESP_OK;
}

bool time_sync_is_synced(void)
{
    return sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED;
}
