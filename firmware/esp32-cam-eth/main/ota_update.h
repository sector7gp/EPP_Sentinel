#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * Empieza una actualización OTA: busca la partición "next" (la que no está
 * corriendo) y abre esp_ota para escribir ahí.
 */
esp_err_t ota_update_begin(void);

/** Escribe un bloque del .bin recibido. Llamar repetidamente tras begin(). */
esp_err_t ota_update_write(const uint8_t *data, size_t len);

/**
 * Cierra la escritura, valida la imagen y la marca como partición de
 * arranque. Si devuelve ESP_OK, el firmware nuevo arranca en el próximo
 * esp_restart().
 */
esp_err_t ota_update_finish(void);

/** Aborta una actualización en curso (error a mitad de la subida). */
void ota_update_abort(void);
