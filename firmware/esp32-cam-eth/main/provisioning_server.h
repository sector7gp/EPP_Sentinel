#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * Arranca mDNS (<prefijo>-<mac>.local) y el servidor HTTP de provisioning.
 * Debe llamarse una vez que net_eth_start() obtuvo IP.
 *
 * Rutas:
 *   GET  /        formulario de alta si no está provisionado, landing si sí
 *   POST /save    guarda backend_url/device_id/device_token en NVS y reinicia
 *   GET  /status  página de solo lectura con el estado operativo
 */
esp_err_t provisioning_server_start(void);

/** Actualiza el estado mostrado en /status tras un intento de subida. */
void provisioning_status_set_capture(bool success, const char *detail);

/** Deja una nota libre visible en /status (ej. errores de red/config). */
void provisioning_status_set_note(const char *note);
