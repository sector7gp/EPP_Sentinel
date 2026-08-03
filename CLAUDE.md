# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

EPP Sentinel is a PPE (Elementos de Protección Personal) compliance detection system. Camera nodes (Raspberry Pi agents or ESP32-S3 firmware) capture images on a schedule, upload them to a central FastAPI backend, which sends them to a multi-provider AI vision API (OpenAI / Anthropic / Gemini) to check whether required PPE is present, and a React panel shows the results. All user-facing text, commit messages, and docs in this repo are in **Spanish** — match that when writing them.

## Architecture

Four independently-deployed components talk over HTTP:

- **`backend/`** — FastAPI + SQLAlchemy + Alembic. Source of truth for devices, streams, profiles, captures, analyses. The only component with a database.
- **`frontend/`** — React + Vite admin panel (dashboard, config, logs). Talks to the backend via `/api/v1`.
- **`agent/`** — Python service for Raspberry Pi nodes. Captures from USB/RTSP, compresses with Pillow, queues, uploads to the backend.
- **`firmware/esp32-cam-eth/`** — ESP-IDF (C) firmware for an ESP32-S3-ETH node with an OV5640 camera. Reimplements the same upload protocol as `agent/` directly in C so no backend changes were needed to support it; see `docs/esp32-firmware.md` for why.
- **`deploy/`** — Docker Compose (`docker-compose.yml`/`.dev.yml`/`.prod.yml`) + nginx reverse proxy, and `deploy/pm2.md` for a non-Docker PM2-based deployment.

Both device types (`agent/` and the ESP32 firmware) authenticate to the backend with an `X-Device-Token` header (the device's `api_token`) and poll `GET /devices/{id}/config` for their capture schedule/resolution/quality, which the backend serves from `Device`/`VideoStream`/`CaptureSchedule`/`ImageSettings` models — changing those settings in the panel takes effect on the device's next config poll, no redeploy needed.

## Backend (`backend/`)

- Entry point `app/main.py`: FastAPI app, CORS from `CORS_ORIGINS` env var, runs Alembic migrations to `head` automatically on startup (`app/migrations.py`), mounts `/storage` as static files for captured images, all API routes under `/api/v1`.
- Two separate auth schemes, both implemented ad hoc (no OAuth library):
  - **Admin/panel**: `POST /auth/login` with `admin_username`/`admin_password` from `Settings` (`app/config.py`, a `pydantic-settings` class reading `.env`) issues a JWT (`app/utils/auth.py`). Admin routes require `Authorization: Bearer <token>`.
  - **Device**: `X-Device-Token` header checked against `Device.api_token` in the DB.
- `app/api/*.py` — one router module per resource (auth, devices, captures, profiles, ai_settings, analyses, audit_logs), included in `main.py` with the `/api/v1` prefix.
- `app/services/*.py` — business logic called from routers (`device_service`, `stream_service`, `profile_service`, `analysis_service`, `audit`). Routers stay thin; put new logic here.
- `app/ai/` — provider abstraction: `base.py` defines the `AIProvider` Protocol (`analyze_image_sync`), `factory.py::get_ai_provider` picks OpenAI/Anthropic/Gemini based on `AISettings.active_provider` and decrypts the stored API key (`app/utils/crypto.py`) — add a new provider by implementing the Protocol and registering it in the factory. `prompt_builder.py` builds the vision prompt from the profile's required EPP list.
- `app/models/models.py` — all SQLAlchemy models in one file: `Device`, `VideoStream` (up to 4 per device, USB or RTSP), `CaptureSchedule`, `ImageSettings`, `OperatorProfile` + `ProfileEPPRequirement` (which EPP types a profile requires) + `DeviceProfileAssignment`, `AISettings` (encrypted API keys), `Capture`, `Analysis`, `AuditLog`.
- `app/constants.py` — `EPPType` enum (the fixed set of detectable PPE items) and `AIProviderName`. Adding a new EPP type means updating this enum plus `EPP_LABELS` and the frontend's mirrored list.
- Migrations: after changing `app/models/models.py`, add an Alembic revision in `backend/alembic/versions/` — migrations run automatically on the next backend start, so a manual `alembic upgrade head` is only needed for scripts/tests.

### Backend commands

```bash
cd backend
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env   # set SECRET_KEY, ADMIN_PASSWORD, and an AI provider key
alembic upgrade head
uvicorn app.main:app --reload --port 8000       # add --host 0.0.0.0 for LAN access
pytest -q                                        # run all tests
pytest tests/test_stream_service.py -q           # single file
pytest tests/test_stream_service.py::test_name   # single test
```

No linter is configured for the backend (no ruff/flake8/black config present).

## Frontend (`frontend/`)

- React 18 + Vite + TypeScript (`strict: true`), React Router, TanStack Query. No CSS framework — one shared `src/styles.css`.
- `src/api/client.ts` — single `request()` wrapper around `fetch`: attaches the JWT from `localStorage`, redirects to `/login` on 401, and unwraps FastAPI's `detail` field (which can be a string or an array of `{msg}` validation errors — see the array-handling there before assuming `err.detail` is always a string).
- `src/pages/` — one file per route: `LoginPage`, `DashboardPage` (live stream grid), `ConfigPage` (devices/streams/profiles/schedule/image quality), `LogsPage` (audit log).
- Dev server proxies `/api` and `/storage` to `http://localhost:8000` (`vite.config.ts`) — the proxy runs on the Vite host, not the browser, so it needs updating only if the backend runs on a *different* machine than the Vite dev server.

### Frontend commands

```bash
cd frontend
npm install
npm run dev      # dev server, add -- --host 0.0.0.0 for LAN access
npm run build    # production build (also what CI runs)
npm run preview
```

No test suite or linter is configured for the frontend (CI only runs `npm run build`).

## Agent (`agent/`, Raspberry Pi)

Small, no-framework Python service: `main.py` (loop), `capture.py` (USB/RTSP capture + Pillow compression), `queue.py` (local pending-upload queue), `scheduler.py` (time-window/interval logic — has a firmware counterpart in `firmware/.../scheduler.c`), `uploader.py` (backend HTTP client with retry backoff), `config.py` (env-based settings via `python-dotenv`). Run with `python main.py --mock-camera` for local testing without hardware.

## Firmware (`firmware/esp32-cam-eth/`, ESP-IDF/C)

Runs on a Waveshare ESP32-S3-ETH-CAM (OV5640 + W5500 Ethernet). Full details, pinout, and OTA instructions are in `firmware/esp32-cam-eth/README.md` — read it before making firmware changes; key points not obvious from a file listing:

- **Partition table** (`partitions.csv`) uses `ota_0`/`ota_1` + `otadata` for real OTA updates (with `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` — the app must call `esp_ota_mark_app_valid_cancel_rollback()` after it proves it booted correctly, done in `app_main.c`). A device still on the old single-partition `factory` layout needs one USB flash to migrate.
- **The web portal** (`main/provisioning_server.c`) is a single `esp_http_server` instance serving hand-written HTML+CSS (no filesystem/SPIFFS assets). `/monitor` and `/settings` are merged into one page: the persistent camera-tuning form saves via `fetch()` so it doesn't interrupt the live-preview `<img>` auto-refresh. `POST /ota` is the only auth-gated route (HTTP Basic, password = the device's own `device_token`).
- **`main/camera.c`** owns a mutex serializing all sensor access (scheduled captures vs. `/capture`/`/monitor` HTTP requests) and tracks the last-applied `framesize_t` to avoid redundant mode switches. When the resolution *does* change, several frames are discarded to let AEC/AGC reconverge before returning a frame — don't reduce that without understanding why (see CHANGELOG v1.5.0).
- **`main/camera_settings.c`** persists sensor tuning (brightness/contrast/AWB/AEC/AGC/etc., not autofocus — unsupported on this board) to NVS and re-applies it through the same mutex as captures.
- The httpd task stack (`config.stack_size` in `provisioning_server_start`) had to be raised well above the ESP-IDF default because the portal's pages use large on-stack string buffers — if you add a page with a big buffer, check this first; the default 4KB stack silently corrupts memory instead of erroring cleanly.
- `docs/esp32-firmware.md` explains the design decision behind why this firmware needed zero backend changes to integrate.

### Firmware commands

```bash
cd firmware/esp32-cam-eth
idf.py set-target esp32s3
idf.py menuconfig      # only needed to change pinout
idf.py build
idf.py -p <port> flash monitor      # first flash (USB)
```

OTA update after the first flash (no USB needed):

```bash
curl -X POST http://<device-ip>/ota -u ":<device_token>" \
  -H "Content-Type: application/octet-stream" \
  --data-binary @build/epp_sentinel_cam.bin
```

## Versioning convention

The whole repo shares one version number across `README.md` (top line), `frontend/package.json`, `backend/app/main.py` (`FastAPI(version=...)`), and `CHANGELOG.md`, regardless of which component actually changed. A release is: bump those, add a `CHANGELOG.md` entry, commit as `Release vX.Y.Z: <summary>`, then `git tag -a vX.Y.Z` and push both. Only bump/tag when the user asks for a release, not on every commit.
