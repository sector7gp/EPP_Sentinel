# Firmware ESP32-S3-ETH (cámara remota)

Firmware ESP-IDF para una cámara remota EPP Sentinel basada en **ESP32-S3 +
Ethernet W5500 (SPI) + sensor OV5640**. Captura y sube imágenes directamente al
backend, reimplementando en C el mismo protocolo que usa `agent/` (Python) en
las Raspberry Pi: no requiere cambios en el backend. Ver el plan de diseño
completo para el contexto de esta decisión.

## Requisitos

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/) v5.1+
- Placa [Waveshare ESP32-S3-ETH](https://www.waveshare.com/wiki/ESP32-S3-ETH) (variante `-CAM`) con OV5640/OV2640 y W5500
- Backend EPP Sentinel accesible en la misma LAN

## Pinout

Confirmado contra la wiki oficial de Waveshare (tablas "Camera control pin
description" y "Network port control pin description" de los demos
`ETH_Web_CAM`/`WIFI_Web_CAM`) y precargado por defecto en
`main/Kconfig.projbuild`. Módulo **ESP32-S3R8**: PSRAM Octal (8MB) y flash
16MB, también confirmados en la captura de configuración de Arduino IDE de
la misma wiki.

| Señal | GPIO | | Señal | GPIO |
|---|---|---|---|---|
| ETH MISO | 12 | | CAM XCLK | 3 |
| ETH MOSI | 11 | | CAM PCLK | 39 |
| ETH SCLK | 13 | | CAM VSYNC | 1 |
| ETH CS | 14 | | CAM HREF | 2 |
| ETH INT | 10 | | CAM SIOD (SDA) | 48 |
| ETH RST | 9 | | CAM SIOC (SCL) | 47 |
| | | | CAM PWDN / `CAM_ENABLE` | 8 (activo en alto) |
| | | | CAM D0..D7 | 41, 45, 46, 42, 40, 38, 15, 18 |

El pin PWDN no aparece en la tabla resumida de la wiki (solo lista las
señales DVP), pero el demo oficial `WIFI_Web_CAM` lo usa en código como
`CAM_ENABLE`; el valor GPIO8 está corroborado además por una configuración
ESPHome funcional de terceros para la misma placa. GPIO33-37 están
reservados internamente (PSRAM octal) y no deben usarse para otra cosa.

Fuentes: [Waveshare ESP32-S3-ETH wiki](https://www.waveshare.com/wiki/ESP32-S3-ETH),
[esquemático](https://files.waveshare.com/wiki/ESP32-S3-ETH/ESP32-S3-ETH-Schematic.pdf).

## Build y flasheo

```bash
cd firmware/esp32-cam-eth
idf.py set-target esp32s3
idf.py menuconfig      # completar pinout (ver arriba)
idf.py build
idf.py -p <puerto> flash monitor
```

## Provisioning (alta de un dispositivo)

1. En el panel web de EPP Sentinel: Configuración → Nodos → crear nodo nuevo.
   Copiar el `device_id` y el `api_token` generados.
2. Flashear el firmware y encender la placa conectada por Ethernet.
3. En el log serial aparece la URL mDNS, ej. `http://epp-cam-a1b2c3.local/`
   (si el equipo desde el que se navega no resuelve mDNS, usar directamente
   la IP que aparece en el log de arranque).
4. Abrir esa URL, completar `backend_url` (IP:puerto del backend en la LAN),
   `device_id` y `device_token`, guardar. El dispositivo reinicia solo y
   entra en operación normal.
5. `http://<host>/status` muestra el estado de la última captura/subida en
   cualquier momento. `/monitor` da una vista en vivo (auto-refresh cada
   0.5s), con preset de resolución/calidad (no toca la config real de
   captura, esa la maneja el backend), botón para forzar una captura+subida
   real ahora mismo, y el formulario de ajustes de imagen del sensor
   (brillo/contraste/AWB/AEC/AGC/etc, ver `camera_settings.h`), que sí
   persiste y se guarda sin recargar la página. `/settings` redirige ahí.

## Actualizar firmware por OTA

Requiere haber flasheado ya una vez la versión con particiones `ota_0`/`ota_1`
(ver `partitions.csv`) — si el dispositivo todavía tiene la partición
`factory` de una versión anterior, hace falta un flasheo por USB único para
migrar.

```bash
idf.py build
curl -X POST http://<host>/ota \
  -u ":<device_token>" \
  -H "Content-Type: application/octet-stream" \
  --data-binary @build/epp_sentinel_cam.bin
```

O desde `http://<host>/ota` en el navegador: elegir el `.bin` y pegar el
`device_token` del dispositivo como contraseña. El dispositivo escribe la
imagen en la partición inactiva, la marca como próximo arranque y reinicia
solo. Si el firmware nuevo no llega a confirmar que arrancó bien (no llega a
inicializar red + cámara), el bootloader vuelve solo a la versión anterior
en el siguiente reset (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`).

## Alcance de esta primera versión

- Sin TLS: asume backend en la misma LAN (HTTP plano), igual que la Raspberry.
- Sin cola persistente multi-item: se reintenta la última captura fallida con
  backoff exponencial hasta que el siguiente intervalo la reemplaza por una
  nueva (ver `main/app_main.c`).
- No implementa Hik-Connect (fuera de alcance del backend actual).

## Estructura

| Archivo | Responsabilidad | Equivalente en `agent/` (Python) |
|---|---|---|
| `main/net_eth.*` | Bring-up Ethernet W5500 + reconexión | — |
| `main/time_sync.*` | SNTP (el ESP32 no tiene RTC con batería) | — |
| `main/device_config.*` | NVS: `device_id`/`device_token`/`backend_url` | `agent/config.py` |
| `main/provisioning_server.*` | Portal web de alta + mDNS + `/status` | — |
| `main/scheduler.*` | Franja horaria / intervalo de captura | `agent/scheduler.py` |
| `main/backend_client.*` | `GET /config`, `POST /captures`, backoff | `agent/uploader.py` |
| `main/camera.*` | Captura JPEG OV5640 + ajuste por `max_kb` | `agent/capture.py` |
| `main/camera_settings.*` | Brillo/contraste/AWB/AEC/AGC/etc del sensor, persistente en NVS | — |
| `main/ota_update.*` | Escritura a la partición OTA (`esp_ota_ops`) | — |
| `main/app_main.c` | Orquestación del loop principal + watchdog | `agent/main.py` |
