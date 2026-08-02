#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t camera_init(void);

/**
 * Captura un frame JPEG intentando respetar max_kb (reintenta bajando
 * calidad hasta 4 veces). *out_buf apunta al framebuffer interno del
 * driver: válido hasta llamar a camera_release().
 *
 * Serializa el acceso a la cámara con un mutex interno: si otra tarea
 * (p.ej. el loop principal subiendo una captura programada, o el handler
 * HTTP de /capture) tiene un frame pendiente, esta llamada bloquea hasta
 * que se libere con camera_release(). Todo llamador de camera_capture_jpeg
 * que reciba ESP_OK debe llamar a camera_release() para no dejar el mutex
 * tomado.
 */
esp_err_t camera_capture_jpeg(int width, int height, int quality, int max_kb,
                               const uint8_t **out_buf, size_t *out_len);

/** Libera el framebuffer devuelto por camera_capture_jpeg y el mutex de captura. */
void camera_release(void);
