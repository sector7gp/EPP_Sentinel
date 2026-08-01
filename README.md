# EPP Sentinel

**Versión 1.2.1** — Sistema autónomo de detección de Elementos de Protección Personal (EPP) con Raspberry Pi, backend central y panel de administración.

## Arquitectura

- **agent/** – Servicio ligero en Raspberry Pi (captura, compresión, cola, subida HTTPS)
- **firmware/esp32-cam-eth/** – Firmware ESP-IDF para cámara remota ESP32-S3-ETH (Ethernet + OV5640), sube directamente al backend
- **backend/** – API FastAPI, análisis IA multi-proveedor, persistencia
- **frontend/** – Panel React de configuración y dashboard
- **deploy/** – Docker Compose y nginx

## Inicio rápido (desarrollo)

### 1. Backend

```bash
cd backend
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
# Editar .env: SECRET_KEY, ADMIN_PASSWORD, OPENAI_API_KEY (opcional)
alembic upgrade head
uvicorn app.main:app --reload --port 8000          # solo accesible desde localhost
```

API: http://localhost:8000/docs  
Admin por defecto: `admin` / `admin` (cambiar en `.env`)

#### Acceder desde otro equipo (p. ej. la Raspberry)

Por defecto uvicorn escucha solo en `127.0.0.1`. Para que otros hosts de la red
(la Raspberry, otro PC) puedan conectarse, enlaza a todas las interfaces con
`--host 0.0.0.0`:

```bash
uvicorn app.main:app --port 8000 --host 0.0.0.0
```

Luego, en el agente, apunta `BACKEND_URL` a la IP del servidor (no `localhost`):

```
BACKEND_URL=http://192.168.1.50:8000   # IP del servidor en la LAN
```

Comprueba conectividad desde la Raspberry: `curl http://192.168.1.50:8000/docs`.
Si no responde, revisa el firewall del servidor (abrir el puerto 8000) y que
ambos equipos estén en la misma red. CORS (`CORS_ORIGINS`) **no** afecta al
agente; solo es necesario para abrir el panel web desde otro equipo (añade ahí
la URL del frontend).

### 2. Frontend

```bash
cd frontend
npm install
npm run dev
```

Panel: http://localhost:5173

#### Acceder desde otro equipo

Al igual que uvicorn, Vite por defecto solo escucha en `127.0.0.1`. Para
exponer el panel a otros hosts de la red, arráncalo con `--host`:

```bash
npm run dev -- --host 0.0.0.0
```

Luego accede desde el otro equipo a `http://<IP-del-servidor>:5173`.

El proxy de `/api` y `/storage` en `frontend/vite.config.ts` reenvía las
peticiones a `http://localhost:8000` **desde la máquina donde corre Vite**
(no desde el navegador), así que sigue funcionando sin cambios mientras
backend y frontend-dev corran en el mismo host. Si el backend está en otra
máquina, actualiza el target del proxy con su IP:

```ts
proxy: {
  "/api": "http://192.168.1.50:8000",
  "/storage": "http://192.168.1.50:8000",
},
```

### 3. Agente (simulado sin cámara)

```bash
cd agent
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
# Configurar BACKEND_URL, DEVICE_TOKEN (crear dispositivo en API o seed)
python main.py --mock-camera
```

### Docker Compose

```bash
cd deploy
docker compose up --build
```

## Documentación

- [Changelog](CHANGELOG.md)
- [Configuración Raspberry Pi](docs/raspberry-setup.md)
- [Cámara remota ESP32-S3-ETH](docs/esp32-firmware.md)
- [API REST](docs/api.md)
- [Ejecutar con PM2 (sin Docker)](deploy/pm2.md)
- Requisitos funcionales: [PromptInicial.md](PromptInicial.md)

### Novedades v1.1.0

- Hasta 4 streams USB/RTSP por nodo Raspberry Pi, cada uno con perfil EPP propio.
- Dashboard en vivo con modal de ampliación y EPP según perfil del stream.
- Tras actualizar: `cd backend && alembic upgrade head` (o reiniciar el backend; aplica migraciones al arrancar).

## Proveedores IA

OpenAI, Anthropic Claude y Google Gemini. Configurables desde el panel sin reiniciar el servidor.
