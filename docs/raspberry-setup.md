# Configuración Raspberry Pi – EPP Sentinel

## Hardware

- Raspberry Pi Zero 2 W
- Webcam USB HD
- Fuente 5V estable
- WiFi configurado

## Sistema operativo

1. Instalar **Raspberry Pi OS Lite** (64-bit recomendado).
2. Habilitar SSH y configurar WiFi con `raspi-config` o Imager.
3. Actualizar:

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y python3-venv python3-pip fswebcam ffmpeg
```

> `ffmpeg` solo es necesario si algún stream configurado en el panel usa RTSP.

Dependencias de runtime para Pillow (procesamiento de imágenes):

```bash
sudo apt install -y \
  libopenjp2-7 \
  libjpeg62-turbo \
  zlib1g \
  libtiff6
```

Para Pi OS Bookworm, opcionalmente: `sudo apt install -y libcamera-apps`

## Instalación del agente

```bash
sudo mkdir -p /opt/epp-sentinel
sudo cp -r agent /opt/epp-sentinel/
cd /opt/epp-sentinel/agent
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
```

## Registrar dispositivo en el backend

1. Iniciar el backend y el panel web.
2. Iniciar sesión como administrador.
3. En **Configuración → Nodos Raspberry Pi**, crear un nodo y copiar:
   - `DEVICE_ID` (UUID)
   - `DEVICE_TOKEN`

Editar `/opt/epp-sentinel/agent/.env`:

```
BACKEND_URL=https://tu-servidor.com
DEVICE_ID=<uuid>
DEVICE_TOKEN=<token>
CONFIG_POLL_SECONDS=300
```

## Fuentes de video (streams)

Cada Raspberry Pi puede gestionar hasta **4 streams** simultáneos. La configuración se define en el panel (**Configuración → Streams**), no en el `.env` del agente. El agente consulta la config remota periódicamente y captura cada stream activo dentro del horario configurado.

| Tipo | Parámetro en el panel | Requisito en la Pi |
|------|----------------------|-------------------|
| **USB** | Dispositivo, ej. `/dev/video0` | `fswebcam` (o `libcamera-still`) |
| **RTSP** | URL completa `rtsp://...` | `ffmpeg` |

Ejemplos de configuración en el panel:

- USB: dispositivo `/dev/video0`, `/dev/video1`, etc.
- RTSP: `rtsp://admin:Password@10.10.7.129:554`

Cada stream tiene un **perfil EPP** propio. Varios streams pueden compartir el mismo perfil. Cambiar perfil o parámetros no requiere reiniciar el agente.

> `ffmpeg` es necesario solo si algún stream del nodo usa RTSP.

### Legacy: `CAMERA_SOURCE` en `.env`

Si el backend no devuelve streams configurados, el agente puede usar `CAMERA_SOURCE` como respaldo temporal (una sola fuente). Se recomienda configurar streams desde el panel.

```
# Solo fallback; preferir configuración remota
CAMERA_SOURCE=rtsp://admin:Password@10.10.7.129:554
```

Probar un stream RTSP manualmente:

```bash
ffmpeg -rtsp_transport tcp -i "rtsp://admin:Password@10.10.7.129:554" -frames:v 1 prueba.jpg
```

## Probar cámara

```bash
cd /opt/epp-sentinel/agent
source .venv/bin/activate
python main.py --test-camera
# Sin hardware: python main.py --test-camera --mock-camera
```

## Servicio systemd

```bash
sudo cp deploy/systemd/epp-agent.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable epp-agent
sudo systemctl start epp-agent
sudo journalctl -u epp-agent -f
```

## Diagnóstico (verificar envío y conexión)

Si no estás seguro de si el agente está enviando imágenes o hay un error de
conexión, ejecuta el diagnóstico. Verifica la configuración, prueba la
autenticación contra el backend y muestra el estado de la cola de subidas:

```bash
cd /opt/epp-sentinel/agent
source .venv/bin/activate
python main.py --diagnose
```

Interpretación de la cola:

- **pendientes / fallidas** altas y subiendo → el agente captura pero **no logra
  subir** (revisa `BACKEND_URL`, red, certificado TLS).
- **confirmadas** creciendo → las imágenes **sí llegan** al backend.
- **cola vacía** → aún no se ha capturado (revisa el horario en el panel, que
  haya streams activos y que esté dentro de la franja horaria).

El diagnóstico también lista los **streams remotos** configurados para el nodo.

Errores comunes que reporta:

- `Token o DEVICE_ID inválido` → revisa `DEVICE_ID`/`DEVICE_TOKEN` en `.env`.
- `No se pudo conectar a ...` → backend caído, IP/puerto incorrectos o sin red.
- ⚠️ `BACKEND_URL apunta a localhost` → en la Raspberry debe ser la IP o dominio
  del servidor, **no** `localhost`.

Para ver el log en vivo del servicio (incluye errores de subida y de config):

```bash
sudo journalctl -u epp-agent -f
```

## Operación autónoma

El servicio `epp-agent` se reinicia automáticamente tras cortes de energía (`Restart=always`).

## Cola offline

Si no hay red, las capturas se guardan en SQLite local (`queue.db`) y se reintentan con backoff exponencial.
