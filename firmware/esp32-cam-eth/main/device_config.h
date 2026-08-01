#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    char backend_url[128];
    char device_id[64];
    char device_token[96];
    bool provisioned;
} device_config_t;

/** Inicializa NVS y carga la configuración persistida (si existe). */
esp_err_t device_config_init(void);

/** true si ya se cargaron backend_url/device_id/device_token válidos. */
bool device_config_is_provisioned(void);

/** Config actual en memoria (válida tras device_config_init). */
const device_config_t *device_config_get(void);

/**
 * Persiste la configuración en NVS y la deja activa en memoria.
 * Marca provisioned=true.
 */
esp_err_t device_config_save(const char *backend_url, const char *device_id,
                              const char *device_token);
