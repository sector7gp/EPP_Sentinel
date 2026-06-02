# API REST – EPP Sentinel

Base URL: `/api/v1`

## Autenticación

### Admin (panel web)

`POST /auth/login`

```json
{ "username": "admin", "password": "admin" }
```

Respuesta: `{ "access_token": "...", "token_type": "bearer" }`

Incluir en rutas admin: `Authorization: Bearer <token>`

### Agente Raspberry Pi

Header: `X-Device-Token: <api_token del dispositivo>`

---

## Agente

| Método | Ruta | Auth | Descripción |
|--------|------|------|-------------|
| POST | `/captures` | Device token | Subir imagen (multipart) |
| GET | `/devices/{id}/config` | Device token o Admin | Config remota del nodo |

### Subir captura

`POST /captures`

| Campo | Tipo | Obligatorio | Descripción |
|-------|------|-------------|-------------|
| `file` | archivo | Sí | Imagen JPEG |
| `stream_id` | form field | No | UUID del stream que originó la captura |

Ejemplo con `curl`:

```bash
curl -X POST "https://servidor/api/v1/captures" \
  -H "X-Device-Token: <token>" \
  -F "file=@captura.jpg" \
  -F "stream_id=<uuid-del-stream>"
```

### Config remota del agente

`GET /devices/{id}/config`

Respuesta (campos relevantes para el agente):

```json
{
  "device_id": "uuid",
  "config_version": "hash-de-config",
  "schedule": {
    "start_time": "07:00:00",
    "end_time": "18:00:00",
    "interval_value": 5,
    "interval_unit": "minutes",
    "enabled_days": "0,1,2,3,4,5,6"
  },
  "image_settings": {
    "width": 1280,
    "height": 720,
    "jpeg_quality": 75,
    "max_kb": 500
  },
  "streams": [
    {
      "id": "uuid",
      "name": "Entrada planta",
      "enabled": true,
      "source_type": "usb",
      "connection_config": { "device": "/dev/video0" },
      "profile_id": "uuid-perfil"
    },
    {
      "id": "uuid",
      "name": "Cámara IP",
      "enabled": true,
      "source_type": "rtsp",
      "connection_config": { "url": "rtsp://user:pass@10.0.0.1:554/stream" },
      "profile_id": "uuid-perfil"
    }
  ]
}
```

El agente solo recibe streams con `enabled: true`. El campo `config_version` cambia cuando se modifica horario, imagen o cualquier stream; el agente detecta el cambio en el próximo poll.

Tipos de origen soportados:

| `source_type` | `connection_config` | Notas |
|---------------|---------------------|-------|
| `usb` | `{ "device": "/dev/video0" }` | Webcam USB; default `/dev/video0` |
| `rtsp` | `{ "url": "rtsp://..." }` | Requiere `ffmpeg` en la Raspberry |

> **Hik-Connect** no está implementado aún; quedará como tipo de origen adicional.

---

## Nodos Raspberry Pi

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/devices` | Listar nodos (nombre, ubicación, estado online, cantidad de streams) |
| POST | `/devices` | Registrar nodo |
| PATCH | `/devices/{id}` | Editar nombre y ubicación |
| DELETE | `/devices/{id}` | Eliminar nodo y datos asociados |
| PUT | `/devices/{id}/schedule` | Horario de captura (compartido por todos los streams del nodo) |
| PUT | `/devices/{id}/image-settings` | Resolución y compresión (compartida por todos los streams) |

### Crear nodo

`POST /devices`

```json
{
  "name": "Pi Planta Norte",
  "location": "Nave 2 - acceso principal"
}
```

Respuesta incluye `id`, `api_token`, `online`, `stream_count`. Al crear un nodo se genera automáticamente un stream USB por defecto (`/dev/video0`) con el perfil EPP predeterminado.

### Editar nodo

`PATCH /devices/{id}`

```json
{
  "name": "Pi Planta Norte",
  "location": "Nave 2"
}
```

Estado **online**: `true` si el agente consultó la config o subió una captura en los últimos 5 minutos (`last_seen_at`).

---

## Streams de video

Cada nodo admite hasta **4 streams** activos o inactivos. Cada stream tiene su propio perfil EPP.

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/devices/{id}/streams` | Listar streams del nodo |
| POST | `/devices/{id}/streams` | Alta de stream |
| PUT | `/devices/{id}/streams/{stream_id}` | Editar stream |
| DELETE | `/devices/{id}/streams/{stream_id}` | Baja de stream |

### Crear stream

`POST /devices/{id}/streams`

USB:

```json
{
  "name": "Webcam entrada",
  "source_type": "usb",
  "connection_config": { "device": "/dev/video0" },
  "profile_id": "uuid-perfil",
  "enabled": true
}
```

RTSP:

```json
{
  "name": "Cámara IP soldadura",
  "source_type": "rtsp",
  "connection_config": { "url": "rtsp://admin:pass@192.168.1.100:554/Streaming/Channels/101" },
  "profile_id": "uuid-perfil",
  "enabled": true
}
```

Validaciones al guardar:

- Máximo 4 streams por nodo
- `source_type` debe ser `usb` o `rtsp`
- USB: `device` no vacío (default `/dev/video0`)
- RTSP: `url` debe comenzar con `rtsp://` o `rtsps://`
- `profile_id` debe existir

Cambiar el perfil o parámetros de un stream **no interrumpe** la ingesta; el agente aplica la nueva config en el siguiente poll.

---

## Perfiles EPP

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/profiles` | Listar perfiles |
| POST | `/profiles` | Crear perfil |
| PUT | `/profiles/{id}` | Editar perfil |
| DELETE | `/profiles/{id}` | Eliminar perfil |

Un mismo perfil puede asignarse a varios streams.

Tipos EPP válidos: `casco_seguridad`, `gafas_seguridad`, `proteccion_auditiva`, `guantes_seguridad`, `calzado_seguridad`, `ropa_industrial`, `proteccion_respiratoria`, `chaleco_reflectivo`.

---

## IA

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/ai-settings` | Proveedor activo y estado de API keys |
| PUT | `/ai-settings` | Cambiar proveedor, modelo o keys |

Proveedores: `openai`, `anthropic`, `gemini`.

---

## Análisis y dashboard

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/analyses/dashboard/streams` | Vista en vivo: todos los streams con último análisis y EPP del perfil |
| GET | `/analyses` | Historial paginado |
| GET | `/analyses/export` | Export CSV |

### Dashboard de streams

`GET /analyses/dashboard/streams`

Devuelve un array con un ítem por stream configurado:

```json
[
  {
    "stream_id": "uuid",
    "stream_name": "Entrada planta",
    "device_id": "uuid",
    "device_name": "Pi Planta Norte",
    "device_location": "Nave 2",
    "device_online": true,
    "profile_id": "uuid",
    "profile_name": "Operario de Planta",
    "required_epp": ["casco_seguridad", "chaleco_reflectivo"],
    "latest_analysis": {
      "id": "uuid",
      "stream_id": "uuid",
      "stream_name": "Entrada planta",
      "image_url": "/storage/captures/.../....jpg",
      "cumple_normativa": false,
      "epp_results": { "casco_seguridad": true, "chaleco_reflectivo": false },
      "required_epp": ["casco_seguridad", "chaleco_reflectivo"],
      "observaciones": "Sin chaleco reflectivo",
      "analyzed_at": "2026-06-01T12:00:00"
    }
  }
]
```

`epp_results` en las respuestas de análisis incluye **solo** los EPP definidos en el perfil del stream.

### Historial

`GET /analyses`

| Query param | Tipo | Descripción |
|-------------|------|-------------|
| `page` | int | Página (default 1) |
| `page_size` | int | Tamaño (default 20, máx. 100) |
| `cumple` | bool | Filtrar por cumplimiento |
| `search` | string | Buscar en observaciones, dispositivo, JSON |
| `from_date` | ISO datetime | Desde |
| `to_date` | ISO datetime | Hasta |
| `stream_id` | UUID | Filtrar por stream |

---

## Auditoría

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/audit-logs` | Logs (`event_type`, `limit`, `offset`) |

---

## Health

`GET /health` → `{ "status": "ok" }`

---

## Imágenes

Las capturas se sirven en:

```
/storage/captures/{device_id}/{capture_id}.jpg
```

---

## Migraciones

Tras actualizar el backend, aplicar migraciones:

```bash
cd backend && alembic upgrade head
```

La migración `002` crea la tabla `video_streams`, añade `location` a dispositivos y migra nodos existentes a un stream USB por defecto.

---

## Endpoints deprecados

| Método | Ruta | Sustituto |
|--------|------|-----------|
| PUT | `/devices/{id}/profile` | Asignar `profile_id` en cada stream |

La asignación de perfil EPP es **por stream**, no por dispositivo.
