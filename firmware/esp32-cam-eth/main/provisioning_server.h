#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * Arranca mDNS (<prefijo>-<mac>.local) y el servidor HTTP de provisioning.
 * Debe llamarse una vez que net_eth_start() obtuvo IP.
 *
 * Rutas:
 *   GET  /         formulario de alta si no está provisionado, landing si sí
 *   POST /save     guarda backend_url/device_id/device_token en NVS y reinicia
 *   GET  /status   página de solo lectura con el estado operativo
 *   GET  /capture  snapshot JPEG único (?resolution=WxH&quality=&max_kb=)
 *   GET  /monitor  vista con auto-refresh sobre /capture
 *   GET  /settings formulario para elegir los parámetros de /monitor
 *                  (no persiste: la config real de captura la manda el backend)
 *   GET  /ota      formulario para subir un firmware .bin nuevo
 *   POST /ota      recibe el .bin y escribe la partición OTA; requiere
 *                  Authorization: Basic base64(":" + device_token)
 */
esp_err_t provisioning_server_start(void);

/** Actualiza el estado mostrado en /status tras un intento de subida. */
void provisioning_status_set_capture(bool success, const char *detail);

/** Deja una nota libre visible en /status (ej. errores de red/config). */
void provisioning_status_set_note(const char *note);
