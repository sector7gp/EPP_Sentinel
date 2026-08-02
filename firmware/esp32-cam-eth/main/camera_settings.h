#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "sensor.h"

// Todos los parámetros de imagen del OV5640 que expone el driver
// esp32-camera, salvo autofoco (no soportado en esta placa/Kconfig). Se
// persisten en NVS y se re-aplican en cada boot, así el sensor queda en un
// estado conocido en vez de depender de la tabla de registros de fábrica.
typedef struct {
    // Imagen
    int8_t brightness;   // -3..3
    int8_t contrast;     // -3..3
    int8_t saturation;   // -4..4
    int8_t sharpness;    // -3..3
    uint8_t denoise;      // 0..8

    // Balance de blancos
    bool whitebal;        // AWB automático
    uint8_t wb_mode;       // 0=Auto 1=Sunny 2=Cloudy 3=Office 4=Home

    // Exposición
    bool exposure_ctrl;   // AEC automático
    bool aec2;             // algoritmo AEC mejorado (recomendado con AEC on)
    int8_t ae_level;      // -5..5, compensación de exposición

    // Ganancia
    bool gain_ctrl;        // AGC automático
    uint8_t agc_gain;       // 0..64, ganancia manual (solo si gain_ctrl=false)
    uint8_t gainceiling;    // gainceiling_t: 0=2X .. 6=128X

    // Orientación
    bool hmirror;
    bool vflip;

    // Corrección
    bool lenc;   // corrección de viñeteado por lente
    bool bpc;    // corrección de píxeles defectuosos (negros)
    bool wpc;    // corrección de píxeles defectuosos (blancos)

    // Efecto especial: 0=Ninguno 1=Negativo 2=Escala de grises 3=Rojo
    // 4=Verde 5=Azul 6=Sepia
    uint8_t special_effect;
} camera_settings_t;

/** Carga los ajustes desde NVS (o los defaults si no hay nada guardado). */
esp_err_t camera_settings_init(void);

/** Ajustes actuales en memoria. */
const camera_settings_t *camera_settings_get(void);

/** Persiste en NVS y actualiza la copia en memoria. No aplica al sensor. */
esp_err_t camera_settings_save(const camera_settings_t *settings);

/** Aplica los ajustes en memoria al sensor vía sensor_t. */
void camera_settings_apply(sensor_t *sensor);
