# Cámara remota ESP32-S3-ETH – EPP Sentinel

Nodo de cámara alternativo a la Raspberry Pi: un **ESP32-S3** con Ethernet
**W5500** (SPI) y sensor **OV5640** que captura y sube imágenes directamente
al backend, sin pasar por `agent/`. El código fuente vive en
[`firmware/esp32-cam-eth/`](../firmware/esp32-cam-eth/) (proyecto ESP-IDF).

Para las instrucciones de build, flasheo y pinout, ver el
[README del firmware](../firmware/esp32-cam-eth/README.md).

## Por qué no requiere cambios de backend

El backend modela cada nodo como un `Device` (ver [`docs/api.md`](api.md#nodos-raspberry-pi))
con `api_token` y streams. El campo `connection_config` de un stream describe
cómo un **agente externo** llega a la fuente de video (USB/RTSP). En el
ESP32, la fuente y el agente son el mismo dispositivo: la placa captura y
sube la imagen ella misma, así que ese campo no aplica. El firmware
reimplementa en C el mismo contrato HTTP que usa `agent/` (Python):

- `GET /api/v1/devices/{device_id}/config` con `X-Device-Token` → horario,
  calidad de imagen y streams habilitados.
- `POST /api/v1/captures` (multipart, `X-Device-Token`) → sube el JPEG,
  usando el `id` del primer stream habilitado como `stream_id`.

## Alta de un dispositivo ESP32

1. Panel web → **Configuración → Nodos** → crear nodo (igual que para una
   Raspberry). Copiar `device_id` y `api_token`.
2. Flashear el firmware (ver README del firmware) y conectar la placa por
   Ethernet a la misma LAN que el backend.
3. Abrir el portal de provisioning de la placa (mDNS `http://epp-cam-xxxx.local/`
   o la IP que muestra el log serial) y cargar `backend_url`, `device_id`,
   `device_token`. La placa reinicia sola y queda operativa.
4. Verificar en el dashboard (`GET /analyses/dashboard/streams` o el panel)
   que las capturas del nuevo nodo llegan y se analizan.

## Diferencias frente al agente Raspberry Pi

| Aspecto | Raspberry (`agent/`) | ESP32-S3-ETH |
|---|---|---|
| Múltiples streams por nodo | Sí, hasta 4 (USB/RTSP) | No; un nodo ESP32 = una cámara |
| Cola offline | SQLite persistente, múltiples ítems | Un único frame pendiente en RAM con backoff; un intervalo nuevo lo reemplaza |
| Compresión JPEG | Recompresión iterativa con Pillow | JPEG nativo del sensor OV5640, ajuste de calidad iterativo |
| Reloj | Reloj del SO | SNTP al arrancar (sin RTC con batería) |
| Actualización de firmware | `git pull` + reinicio de servicio | Flasheo por USB (sin OTA en esta versión) |

## Pendiente / fuera de alcance de esta versión

- OTA de firmware.
- TLS/HTTPS (asume backend en la misma LAN).
- Integración Hik-Connect (tampoco implementada del lado backend, ver
  [`docs/api.md`](api.md#agente)).
