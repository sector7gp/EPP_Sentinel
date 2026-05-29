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

## Agente

| Método | Ruta | Auth | Descripción |
|--------|------|------|-------------|
| POST | `/captures` | Device token | Subir imagen (multipart `file`) |
| GET | `/devices/{id}/config` | Device token o Admin | Config remota |

## Administración

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET/POST | `/devices` | Listar / registrar dispositivos |
| PUT | `/devices/{id}/schedule` | Horario de captura |
| PUT | `/devices/{id}/image-settings` | Resolución y compresión |
| PUT | `/devices/{id}/profile` | Asignar perfil EPP |
| GET/POST/PUT/DELETE | `/profiles` | Perfiles de operario |
| GET/PUT | `/ai-settings` | Proveedor y API keys |
| GET | `/analyses` | Historial (filtros: `cumple`, `search`, `from_date`, `to_date`, `page`) |
| GET | `/analyses/export` | Export CSV |
| GET | `/audit-logs` | Logs (`event_type`, `limit`, `offset`) |

## Health

`GET /health` → `{ "status": "ok" }`

## Imágenes

Las capturas se sirven en `/storage/captures/{device_id}/{capture_id}.jpg`
