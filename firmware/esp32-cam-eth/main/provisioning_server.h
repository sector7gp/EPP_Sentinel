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
 *   GET  /capture     snapshot JPEG único (?resolution=WxH&quality=&max_kb=)
 *   GET  /monitor     vista con auto-refresh sobre /capture (preset de
 *                     resolución, no persiste) + formulario de ajustes de
 *                     imagen del sensor (brillo/contraste/AWB/AEC/AGC/etc,
 *                     ver camera_settings.h; ese sí persiste en NVS, se
 *                     guarda por fetch() sin recargar la página) + botón
 *                     para forzar una captura+subida real
 *   GET  /settings    alias: redirige a /monitor (ahí quedó todo junto)
 *   POST /settings    guarda y aplica el formulario de ajustes de imagen
 *   POST /capture-now captura con la config remota actual y la sube al
 *                     backend ya, sin esperar el próximo intervalo
 *   GET  /ota         formulario para subir un firmware .bin nuevo
 *   POST /ota      recibe el .bin y escribe la partición OTA; requiere
 *                  Authorization: Basic base64(":" + device_token)
 */
esp_err_t provisioning_server_start(void);

/** Actualiza el estado mostrado en /status tras un intento de subida. */
void provisioning_status_set_capture(bool success, const char *detail);

/** Deja una nota libre visible en /status (ej. errores de red/config). */
void provisioning_status_set_note(const char *note);
