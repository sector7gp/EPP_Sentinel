#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * Inicializa el bus SPI y el driver W5500, y arranca la pila de red.
 * Bloquea hasta obtener IP por DHCP o hasta agotar el timeout (ms).
 * Devuelve ESP_OK si se obtuvo IP, ESP_ERR_TIMEOUT si no.
 *
 * Los pines se toman de Kconfig (EPP_ETH_GPIO_*); si alguno no está
 * configurado (-1) devuelve ESP_ERR_INVALID_STATE sin tocar hardware.
 */
esp_err_t net_eth_start(uint32_t timeout_ms);

/** true si el link está actualmente arriba y hay IP asignada. */
bool net_eth_is_connected(void);
