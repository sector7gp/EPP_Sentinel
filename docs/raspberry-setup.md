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

> `ffmpeg` solo es necesario si se usa una cámara IP/RTSP (ver `CAMERA_SOURCE`).

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
3. En **Configuración → Dispositivos**, crear un dispositivo y copiar:
   - `DEVICE_ID` (UUID)
   - `DEVICE_TOKEN`

Editar `/opt/epp-sentinel/agent/.env`:

```
BACKEND_URL=https://tu-servidor.com
DEVICE_ID=<uuid>
DEVICE_TOKEN=<token>
CONFIG_POLL_SECONDS=300
```

## Fuente de cámara

Por defecto el agente captura desde una webcam USB local (`fswebcam`, con
`libcamera-still` como respaldo). Para usar una **cámara IP/RTSP**, indicar la
URL completa en `CAMERA_SOURCE` (requiere `ffmpeg`):

```
CAMERA_SOURCE=rtsp://admin:Password@10.10.7.129:554
```

El agente extrae un fotograma del stream con `ffmpeg` (transporte TCP) en cada
captura, lo escala a la resolución configurada y lo comprime como JPEG. Si
`CAMERA_SOURCE` queda vacío, se usa la webcam local.

Probar el stream manualmente:

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

## Operación autónoma

El servicio `epp-agent` se reinicia automáticamente tras cortes de energía (`Restart=always`).

## Cola offline

Si no hay red, las capturas se guardan en SQLite local (`queue.db`) y se reintentan con backoff exponencial.
