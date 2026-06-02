# Changelog

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
