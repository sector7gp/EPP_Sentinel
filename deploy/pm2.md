# Ejecutar con PM2 (backend, frontend y agente separados)

Alternativa a Docker Compose para correr los tres servicios como procesos
gestionados por [PM2](https://pm2.keymetrics.io/), útil cuando se ejecutan
sobre un host ya provisto con sus propios entornos virtuales de Python
(`backend/.venv`, `agent/venv`).

Cada proceso apunta directo al binario/intérprete dentro de su `venv`, así
PM2 no depende de que el shell tenga un entorno virtual activado — usa
siempre ese intérprete fijo, sin importar el `PATH` del momento.

Correr todos los comandos desde la raíz del repo.

## Requisitos previos

- `backend/.venv` con dependencias instaladas (`pip install -r backend/requirements.txt`) y `backend/.env` configurado.
- `agent/venv` con dependencias instaladas (`pip install -r agent/requirements.txt`) y `agent/.env` configurado (`BACKEND_URL`, `DEVICE_ID`, `DEVICE_TOKEN`).
- `frontend/node_modules` instalado (`npm install` en `frontend/`).
- El puerto del backend usado abajo (`8089`) debe coincidir con el proxy de `frontend/vite.config.ts` (`server.proxy`). Si cambias el puerto del backend, actualiza también ese archivo.

## Backend

Uvicorn desde `backend/.venv`, escuchando en todas las interfaces para
aceptar conexiones desde otros hosts de la LAN. El `.env` se lee relativo al
`--cwd`.

```bash
pm2 start backend/.venv/bin/uvicorn \
  --name epp-backend \
  --cwd backend \
  --interpreter none \
  -- app.main:app --host 0.0.0.0 --port 8089
```

## Frontend

Servidor de desarrollo de Vite, expuesto a la LAN con `--host`.

```bash
pm2 start npm \
  --name epp-frontend \
  --cwd frontend \
  -- run dev -- --host 0.0.0.0
```

## Agente

Python desde `agent/venv`. El `.env` se lee relativo al `--cwd`, igual que
`queue.db` y la carpeta `pending/`.

```bash
pm2 start agent/venv/bin/python \
  --name epp-agent \
  --cwd agent \
  --interpreter none \
  -- main.py
```

## Persistencia y verificación

```bash
pm2 save              # guarda el set de procesos actual
pm2 status
pm2 logs epp-backend   # o epp-frontend / epp-agent
```

Para que PM2 levante estos procesos automáticamente al reiniciar el host,
seguir la guía oficial de `pm2 startup`.
