#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "scheduler.h"

typedef struct {
    schedule_config_t schedule;
    int image_width;
    int image_height;
    int image_jpeg_quality;
    int image_max_kb;
    char stream_id[64];     // primer stream habilitado; vacío si no hay ninguno
    char config_version[64];
    bool valid;
} remote_config_t;

/** GET /api/v1/devices/{device_id}/config (equivalente a agent/uploader.py::fetch_config). */
esp_err_t backend_client_fetch_config(remote_config_t *out);

/**
 * POST /api/v1/captures con el JPEG como multipart/form-data
 * (equivalente a agent/uploader.py::Uploader.upload).
 */
esp_err_t backend_client_upload(const uint8_t *jpeg_data, size_t jpeg_len, const char *stream_id);

/** Réplica de agent/uploader.py::retry_delay: min(300, 2**retries*5) segundos. */
int backend_client_retry_delay_seconds(int retries);
