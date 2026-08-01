#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * Sincroniza el reloj vía SNTP (servidor de Kconfig EPP_NTP_SERVER).
 * Necesario antes de evaluar la franja horaria de captura, ya que el
 * ESP32 no tiene RTC con batería. Bloquea hasta sincronizar o timeout;
 * si hay timeout, esp_netif_sntp sigue reintentando en segundo plano.
 */
esp_err_t time_sync_start(uint32_t timeout_ms);

/** true si la hora del sistema ya se sincronizó al menos una vez. */
bool time_sync_is_synced(void);
