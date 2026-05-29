# EPP Sentinel

Sistema autónomo de detección de Elementos de Protección Personal (EPP) con Raspberry Pi, backend central y panel de administración.

## Arquitectura

- **agent/** – Servicio ligero en Raspberry Pi (captura, compresión, cola, subida HTTPS)
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
uvicorn app.main:app --reload --port 8000
```

API: http://localhost:8000/docs  
Admin por defecto: `admin` / `admin` (cambiar en `.env`)

### 2. Frontend

```bash
cd frontend
npm install
npm run dev
```

Panel: http://localhost:5173

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

- [Configuración Raspberry Pi](docs/raspberry-setup.md)
- [API REST](docs/api.md)
- Requisitos funcionales: [PromptInicial.md](PromptInicial.md)

## Proveedores IA

OpenAI, Anthropic Claude y Google Gemini. Configurables desde el panel sin reiniciar el servidor.
