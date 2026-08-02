# Changelog

## [1.3.0] — 2026-08-01

### Nuevo

- **Portal ESP32-S3-ETH rediseñado**: CSS compartida (paleta del panel web), navegación entre páginas, en vez del HTML pelado anterior.
- **Monitor en vivo** (`/monitor`): snapshot con auto-refresh por JS sobre un nuevo endpoint `GET /capture` (`?resolution=WxH&quality=&max_kb=`).
- **Ajustes de monitor** (`/settings`): presets de resolución con nombre (QVGA…QSXGA, incluye Full HD) para probar `/monitor` sin tocar la config real del backend.
- **Actualización OTA** (`/ota`): sube un `.bin` nuevo desde el navegador (o `curl`), protegido con el `device_token` del propio dispositivo como contraseña (HTTP Basic, usuario libre). Requiere partición de dos slots (`ota_0`/`ota_1` + `otadata`, ver `partitions.csv`) y rollback automático si el firmware nuevo no llega a confirmar que arrancó bien.

### Correcciones

- **Firmware**: la tarea del servidor HTTP corría con el stack por defecto (4096 bytes); las páginas nuevas (con un buffer de cabecera de 2560 bytes) lo desbordaban y crasheaban el equipo en el primer request tras bootear. Subido a 8192.
- **Firmware**: `FRAMESIZE_QSXGA` del driver `esp32-camera` es 2560x1920, no 2592x1944 como asumía el código — pedir "2592x1944" terminaba usando esa resolución más chica sin avisar. Ese resultado (2592x1944, `FRAMESIZE_5MP`) además da timeout de captura sistemático en esta placa a cualquier calidad; el techo real y estable es QSXGA (2560x1920). Backend y panel ajustados al mismo límite.
- **Firmware**: al cambiar de resolución en caliente, el primer frame capturado podía seguir siendo del tamaño anterior (el sensor tarda un frame en aplicar el cambio); ahora se descarta ese frame de transición.
- **Firmware**: si el JPEG no entraba en el buffer DMA, `esp_camera_fb_get()` devolvía NULL y la captura fallaba directo; ahora ese caso reintenta subiendo la compresión igual que cuando el frame sí llega pero pesa de más.
- **Firmware**: la cámara no tenía lock entre el loop de capturas programadas y los nuevos endpoints HTTP que también capturan (`/capture`); se agregó un mutex.

## [1.2.3] — 2026-08-01

### Correcciones

- **Firmware ESP32-S3-ETH**: `camera_init()` reservaba el buffer DMA/JPEG al tamaño de `FRAMESIZE_XGA` (1024x768) sin importar la resolución pedida luego con `sensor->set_framesize()`; capturar a QXGA (2048x1536) o QSXGA (2592x1944) desbordaba ese buffer y `esp_camera_fb_get()` fallaba. Ahora se inicializa con `FRAMESIZE_QSXGA` (el máximo del OV5640) para que el buffer alcance para cualquier resolución configurada. Requiere recompilar y reflashear la placa (`idf.py build && idf.py -p <puerto> flash`).

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
