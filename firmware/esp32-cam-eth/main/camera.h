#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t camera_init(void);

/**
 * Captura un frame JPEG intentando respetar max_kb (reintenta bajando
 * calidad hasta 4 veces). *out_buf apunta al framebuffer interno del
 * driver: válido hasta llamar a camera_release().
 */
esp_err_t camera_capture_jpeg(int width, int height, int quality, int max_kb,
                               const uint8_t **out_buf, size_t *out_len);

/** Libera el framebuffer devuelto por camera_capture_jpeg. */
void camera_release(void);
