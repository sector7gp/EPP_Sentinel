#include "camera_settings.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "camera_settings";
static const char *NVS_NAMESPACE = "epp_cam";

static camera_settings_t s_settings;

static void set_defaults(camera_settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->whitebal = true;
    s->exposure_ctrl = true;
    s->aec2 = true;
    s->gain_ctrl = true;
    s->gainceiling = 1; // GAINCEILING_4X
    s->lenc = true;
    s->bpc = true;
    s->wpc = true;
}

esp_err_t camera_settings_init(void)
{
    set_defaults(&s_settings);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Sin ajustes de cámara guardados, uso defaults (%s)", esp_err_to_name(err));
        return ESP_OK;
    }

    int8_t i8;
    uint8_t u8;
    if (nvs_get_i8(handle, "brightness", &i8) == ESP_OK) s_settings.brightness = i8;
    if (nvs_get_i8(handle, "contrast", &i8) == ESP_OK) s_settings.contrast = i8;
    if (nvs_get_i8(handle, "saturation", &i8) == ESP_OK) s_settings.saturation = i8;
    if (nvs_get_i8(handle, "sharpness", &i8) == ESP_OK) s_settings.sharpness = i8;
    if (nvs_get_u8(handle, "denoise", &u8) == ESP_OK) s_settings.denoise = u8;
    if (nvs_get_u8(handle, "whitebal", &u8) == ESP_OK) s_settings.whitebal = u8;
    if (nvs_get_u8(handle, "wb_mode", &u8) == ESP_OK) s_settings.wb_mode = u8;
    if (nvs_get_u8(handle, "exposure_ctrl", &u8) == ESP_OK) s_settings.exposure_ctrl = u8;
    if (nvs_get_u8(handle, "aec2", &u8) == ESP_OK) s_settings.aec2 = u8;
    if (nvs_get_i8(handle, "ae_level", &i8) == ESP_OK) s_settings.ae_level = i8;
    if (nvs_get_u8(handle, "gain_ctrl", &u8) == ESP_OK) s_settings.gain_ctrl = u8;
    if (nvs_get_u8(handle, "agc_gain", &u8) == ESP_OK) s_settings.agc_gain = u8;
    if (nvs_get_u8(handle, "gainceiling", &u8) == ESP_OK) s_settings.gainceiling = u8;
    if (nvs_get_u8(handle, "hmirror", &u8) == ESP_OK) s_settings.hmirror = u8;
    if (nvs_get_u8(handle, "vflip", &u8) == ESP_OK) s_settings.vflip = u8;
    if (nvs_get_u8(handle, "lenc", &u8) == ESP_OK) s_settings.lenc = u8;
    if (nvs_get_u8(handle, "bpc", &u8) == ESP_OK) s_settings.bpc = u8;
    if (nvs_get_u8(handle, "wpc", &u8) == ESP_OK) s_settings.wpc = u8;
    if (nvs_get_u8(handle, "special_effect", &u8) == ESP_OK) s_settings.special_effect = u8;

    nvs_close(handle);
    ESP_LOGI(TAG, "Ajustes de cámara cargados desde NVS");
    return ESP_OK;
}

const camera_settings_t *camera_settings_get(void)
{
    return &s_settings;
}

esp_err_t camera_settings_save(const camera_settings_t *settings)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open falló: %s", esp_err_to_name(err));
        return err;
    }

    err = err ?: nvs_set_i8(handle, "brightness", settings->brightness);
    err = err ?: nvs_set_i8(handle, "contrast", settings->contrast);
    err = err ?: nvs_set_i8(handle, "saturation", settings->saturation);
    err = err ?: nvs_set_i8(handle, "sharpness", settings->sharpness);
    err = err ?: nvs_set_u8(handle, "denoise", settings->denoise);
    err = err ?: nvs_set_u8(handle, "whitebal", settings->whitebal);
    err = err ?: nvs_set_u8(handle, "wb_mode", settings->wb_mode);
    err = err ?: nvs_set_u8(handle, "exposure_ctrl", settings->exposure_ctrl);
    err = err ?: nvs_set_u8(handle, "aec2", settings->aec2);
    err = err ?: nvs_set_i8(handle, "ae_level", settings->ae_level);
    err = err ?: nvs_set_u8(handle, "gain_ctrl", settings->gain_ctrl);
    err = err ?: nvs_set_u8(handle, "agc_gain", settings->agc_gain);
    err = err ?: nvs_set_u8(handle, "gainceiling", settings->gainceiling);
    err = err ?: nvs_set_u8(handle, "hmirror", settings->hmirror);
    err = err ?: nvs_set_u8(handle, "vflip", settings->vflip);
    err = err ?: nvs_set_u8(handle, "lenc", settings->lenc);
    err = err ?: nvs_set_u8(handle, "bpc", settings->bpc);
    err = err ?: nvs_set_u8(handle, "wpc", settings->wpc);
    err = err ?: nvs_set_u8(handle, "special_effect", settings->special_effect);
    err = err ?: nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudieron guardar los ajustes: %s", esp_err_to_name(err));
        return err;
    }

    s_settings = *settings;
    ESP_LOGI(TAG, "Ajustes de cámara guardados");
    return ESP_OK;
}

void camera_settings_apply(sensor_t *sensor)
{
    if (!sensor) {
        return;
    }
    const camera_settings_t *s = &s_settings;

    if (sensor->set_brightness) sensor->set_brightness(sensor, s->brightness);
    if (sensor->set_contrast) sensor->set_contrast(sensor, s->contrast);
    if (sensor->set_saturation) sensor->set_saturation(sensor, s->saturation);
    if (sensor->set_sharpness) sensor->set_sharpness(sensor, s->sharpness);
    if (sensor->set_denoise) sensor->set_denoise(sensor, s->denoise);

    if (sensor->set_whitebal) sensor->set_whitebal(sensor, s->whitebal);
    if (sensor->set_wb_mode) sensor->set_wb_mode(sensor, s->wb_mode);

    if (sensor->set_exposure_ctrl) sensor->set_exposure_ctrl(sensor, s->exposure_ctrl);
    if (sensor->set_aec2) sensor->set_aec2(sensor, s->aec2);
    if (sensor->set_ae_level) sensor->set_ae_level(sensor, s->ae_level);

    if (sensor->set_gain_ctrl) sensor->set_gain_ctrl(sensor, s->gain_ctrl);
    if (sensor->set_agc_gain) sensor->set_agc_gain(sensor, s->agc_gain);
    if (sensor->set_gainceiling) sensor->set_gainceiling(sensor, (gainceiling_t)s->gainceiling);

    if (sensor->set_hmirror) sensor->set_hmirror(sensor, s->hmirror);
    if (sensor->set_vflip) sensor->set_vflip(sensor, s->vflip);

    if (sensor->set_lenc) sensor->set_lenc(sensor, s->lenc);
    if (sensor->set_bpc) sensor->set_bpc(sensor, s->bpc);
    if (sensor->set_wpc) sensor->set_wpc(sensor, s->wpc);

    if (sensor->set_special_effect) sensor->set_special_effect(sensor, s->special_effect);

    ESP_LOGI(TAG, "Ajustes de cámara aplicados al sensor");
}
