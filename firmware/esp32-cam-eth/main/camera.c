#include "camera.h"

#include "driver/ledc.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "camera";
static camera_fb_t *s_last_fb = NULL;

static framesize_t framesize_for(int width, int height)
{
    (void)height;
    if (width >= 2592) return FRAMESIZE_QSXGA; // 2592x1944 (máximo OV5640)
    if (width >= 2048) return FRAMESIZE_QXGA;  // 2048x1536
    if (width >= 1600) return FRAMESIZE_UXGA;  // 1600x1200
    if (width >= 1280) return FRAMESIZE_HD;    // 1280x720
    if (width >= 1024) return FRAMESIZE_XGA;   // 1024x768
    if (width >= 800) return FRAMESIZE_SVGA;   // 800x600
    if (width >= 640) return FRAMESIZE_VGA;    // 640x480
    return FRAMESIZE_QVGA;                     // 320x240
}

static int map_pillow_quality_to_camera(int pillow_quality)
{
    int clamped = pillow_quality < 10 ? 10 : (pillow_quality > 95 ? 95 : pillow_quality);
    // image_settings.jpeg_quality viene del backend con la convención de Pillow
    // (mayor = mejor calidad, ver agent/capture.py::compress_image). El driver
    // esp32-camera usa la escala inversa: 0 = mejor calidad, 63 = peor.
    int camera_q = 63 - (clamped * 63) / 95;
    if (camera_q < 4) camera_q = 4;
    if (camera_q > 63) camera_q = 63;
    return camera_q;
}

esp_err_t camera_init(void)
{
    if (CONFIG_EPP_CAM_GPIO_XCLK < 0 || CONFIG_EPP_CAM_GPIO_SIOD < 0 ||
        CONFIG_EPP_CAM_GPIO_SIOC < 0 || CONFIG_EPP_CAM_GPIO_VSYNC < 0 ||
        CONFIG_EPP_CAM_GPIO_HREF < 0 || CONFIG_EPP_CAM_GPIO_PCLK < 0 ||
        CONFIG_EPP_CAM_GPIO_D0 < 0 || CONFIG_EPP_CAM_GPIO_D1 < 0 ||
        CONFIG_EPP_CAM_GPIO_D2 < 0 || CONFIG_EPP_CAM_GPIO_D3 < 0 ||
        CONFIG_EPP_CAM_GPIO_D4 < 0 || CONFIG_EPP_CAM_GPIO_D5 < 0 ||
        CONFIG_EPP_CAM_GPIO_D6 < 0 || CONFIG_EPP_CAM_GPIO_D7 < 0) {
        ESP_LOGE(TAG, "Pines de la cámara sin configurar: ejecute 'idf.py menuconfig' "
                       "y complete 'EPP Sentinel Camera > Cámara OV5640 (DVP)' "
                       "con el pinout real de la placa");
        return ESP_ERR_INVALID_STATE;
    }

    camera_config_t config = {
        .pin_pwdn = CONFIG_EPP_CAM_GPIO_PWDN,
        .pin_reset = CONFIG_EPP_CAM_GPIO_RESET,
        .pin_xclk = CONFIG_EPP_CAM_GPIO_XCLK,
        .pin_sccb_sda = CONFIG_EPP_CAM_GPIO_SIOD,
        .pin_sccb_scl = CONFIG_EPP_CAM_GPIO_SIOC,
        .pin_d7 = CONFIG_EPP_CAM_GPIO_D7,
        .pin_d6 = CONFIG_EPP_CAM_GPIO_D6,
        .pin_d5 = CONFIG_EPP_CAM_GPIO_D5,
        .pin_d4 = CONFIG_EPP_CAM_GPIO_D4,
        .pin_d3 = CONFIG_EPP_CAM_GPIO_D3,
        .pin_d2 = CONFIG_EPP_CAM_GPIO_D2,
        .pin_d1 = CONFIG_EPP_CAM_GPIO_D1,
        .pin_d0 = CONFIG_EPP_CAM_GPIO_D0,
        .pin_vsync = CONFIG_EPP_CAM_GPIO_VSYNC,
        .pin_href = CONFIG_EPP_CAM_GPIO_HREF,
        .pin_pclk = CONFIG_EPP_CAM_GPIO_PCLK,
        .xclk_freq_hz = CONFIG_EPP_CAM_XCLK_FREQ_HZ,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        // Reserva el buffer DMA/JPEG al tamaño máximo del sensor una sola vez
        // en el init; sensor->set_framesize() en cada captura solo puede
        // *reducir* la resolución dentro de ese buffer, nunca superarlo.
        .frame_size = FRAMESIZE_QSXGA,
        .jpeg_quality = 12,
        .fb_count = 1,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init falló: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Cámara OV5640 inicializada");
    return ESP_OK;
}

esp_err_t camera_capture_jpeg(int width, int height, int quality, int max_kb,
                               const uint8_t **out_buf, size_t *out_len)
{
    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor) {
        ESP_LOGE(TAG, "Sensor de cámara no disponible");
        return ESP_FAIL;
    }

    sensor->set_framesize(sensor, framesize_for(width, height));

    int camera_quality = map_pillow_quality_to_camera(quality);
    size_t max_bytes = (size_t)(max_kb > 0 ? max_kb : 500) * 1024;

    camera_fb_t *fb = NULL;
    for (int attempt = 0; attempt < 4; attempt++) {
        sensor->set_quality(sensor, camera_quality);
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        }
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGW(TAG, "esp_camera_fb_get devolvió NULL (intento %d)", attempt + 1);
            return ESP_FAIL;
        }
        if (fb->len <= max_bytes || camera_quality >= 55) {
            break;
        }
        camera_quality = (camera_quality + 8) > 63 ? 63 : (camera_quality + 8);
    }

    s_last_fb = fb;
    *out_buf = fb->buf;
    *out_len = fb->len;
    return ESP_OK;
}

void camera_release(void)
{
    if (s_last_fb) {
        esp_camera_fb_return(s_last_fb);
        s_last_fb = NULL;
    }
}
