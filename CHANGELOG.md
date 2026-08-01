# Changelog

## [1.2.2] — 2026-08-01

### Correcciones

- **Backend**: el schema `ImageSettingsUpdate` seguía limitando `width`/`height` a 1920x1080 pese a que el firmware ya soporta hasta 2592x1944 (QXGA/QSXGA), causando `422 Unprocessable Content` al guardar resoluciones altas desde el panel. Límites actualizados a `le=2592`/`le=1944`.
- **Panel**: el formulario "Calidad de imagen" no validaba rangos ni mostraba el motivo del error; ahora los inputs tienen `min`/`max` visibles junto al label.
- **Cliente API**: los errores de validación (422) de FastAPI llegan como lista de objetos (`detail: [{msg, loc, type}]`) y se mostraban como `[object Object]`; ahora se concatenan los mensajes legibles.

## [1.2.1] — 2026-07-31

### Correcciones

- **Firmware ESP32-S3-ETH**: la resolución máxima de captura estaba limitada a UXGA (1600x1200) sin importar lo configurado en el panel; ahora soporta QXGA y QSXGA (hasta 2592x1944, máximo del OV5640).
- **Panel de configuración**: "Horario de captura" y "Calidad de imagen" mostraban siempre los valores por defecto en lugar de los últimos guardados para el dispositivo, y "Guardar imagen" no mostraba confirmación ni errores. Ahora ambos formularios cargan la configuración real del dispositivo (`GET /devices/{id}/config`) y muestran un mensaje de confirmación al guardar.

## [1.2.0] — 2026-07-31

### Nuevo

- **Nodo de cámara ESP32-S3-ETH**: firmware ESP-IDF (`firmware/esp32-cam-eth/`) para un nodo alternativo a la Raspberry Pi, con Ethernet W5500 y sensor OV5640, que sube capturas directamente al backend sin pasar por `agent/`.
- **Despliegue con PM2**: guía y comandos para correr backend, frontend y agente como procesos separados de PM2 sin Docker (`deploy/pm2.md`).

### Cambios

- Documentado el acceso desde otros hosts de la LAN: enlazar uvicorn (`--host 0.0.0.0`) y Vite (`--host`), y ajustar el proxy de `frontend/vite.config.ts` si el backend usa un puerto distinto de 8000.
- Documentación actualizada: `docs/esp32-firmware.md`.

## [1.1.0] — 2026-06-01

### Nuevo

- **Multi-stream por nodo Raspberry Pi**: hasta 4 fuentes simultáneas por dispositivo (USB y RTSP).
- **Perfil EPP por stream**: cada stream tiene su propio perfil; varios streams pueden compartir perfil.
- **Dashboard de monitoreo**: grilla de streams, EPP filtrados por perfil del stream en foco, modal al ampliar miniatura.
- **Configuración reestructurada**: CRUD de nodos (nombre, ubicación, estado online), streams y perfiles EPP.
- **API**: endpoints de streams, dashboard en vivo (`GET /analyses/dashboard/streams`), `PATCH`/`DELETE` de nodos.
- **Agente**: captura multi-stream, subida con `stream_id`, soporte USB por dispositivo (`/dev/videoN`).
- **Migración Alembic 002**: tablas `video_streams`, columna `location` en nodos y `stream_id` en capturas.
- **Migraciones automáticas** al arrancar el backend.

### Cambios

- La asignación de perfil EPP pasa de dispositivo a **stream** (`PUT /devices/{id}/profile` deprecado).
- Documentación actualizada: `docs/api.md`, `docs/raspberry-setup.md`.

### Correcciones

- Importación circular entre servicios de dispositivo y stream.
- Migración SQLite idempotente para esquemas parcialmente aplicados.
- Eliminación de streams y nodos sin errores de sesión SQLAlchemy (`StaleDataError`).

### Pendiente

- Integración **Hik-Connect** (planificada para versión futura).

---

## [1.0.0] — versión inicial

- Captura automática desde webcam o RTSP único vía `CAMERA_SOURCE`.
- Análisis EPP con OpenAI, Anthropic y Gemini.
- Panel de configuración, dashboard histórico, logs y despliegue Docker.
