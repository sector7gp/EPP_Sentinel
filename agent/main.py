#!/usr/bin/env python3
"""EPP Sentinel edge agent for Raspberry Pi."""

import argparse
import logging
import sys
import time
from pathlib import Path

# Allow running as script from agent directory
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from agent.capture import capture_image
from agent.config import load_config
from agent.queue import UploadQueue
from agent.scheduler import interval_seconds, is_within_schedule
from agent.uploader import Uploader, retry_delay

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
logging.getLogger("httpx").setLevel(logging.WARNING)
logger = logging.getLogger("epp-agent")

PENDING_DIR = Path(__file__).parent / "pending"


def resolve_stream_source(stream: dict) -> str:
    cfg = stream.get("connection_config") or {}
    if stream.get("source_type") == "rtsp":
        return cfg.get("url", "")
    return cfg.get("device", "/dev/video0")


def process_queue(queue: UploadQueue, uploader: Uploader) -> None:
    for item_id, file_path, retries, stream_id in queue.pending_items():
        path = Path(file_path)
        if not path.exists():
            queue.mark_acked(item_id)
            continue
        if retries > 0:
            time.sleep(retry_delay(retries))
        queue.mark_uploading(item_id)
        try:
            if uploader.upload(path, stream_id=stream_id):
                queue.mark_acked(item_id)
                path.unlink(missing_ok=True)
                logger.info("Subida exitosa: %s (stream %s)", path.name, stream_id or "-")
            else:
                queue.mark_failed(item_id, retries)
        except Exception as exc:
            logger.warning("Error de subida: %s", exc)
            queue.mark_failed(item_id, retries)


def run_loop(mock_camera: bool = False) -> None:
    config = load_config(mock_camera=mock_camera)
    if not config.device_id or not config.device_token:
        logger.error("Configure DEVICE_ID y DEVICE_TOKEN en .env")
        sys.exit(1)

    PENDING_DIR.mkdir(parents=True, exist_ok=True)
    queue = UploadQueue(config.queue_db)
    uploader = Uploader(config)
    remote_config: dict | None = None
    config_version = ""
    last_config_fetch = 0.0
    last_capture: dict[str, float] = {}

    logger.info("Agente iniciado para dispositivo %s", config.device_id)

    while True:
        now = time.time()
        if now - last_config_fetch >= config.config_poll_seconds:
            remote_config = uploader.fetch_config()
            last_config_fetch = now
            if remote_config:
                new_version = remote_config.get("config_version", "")
                if new_version != config_version:
                    logger.info("Configuración actualizada: %s", new_version)
                    config_version = new_version

        process_queue(queue, uploader)

        streams = remote_config.get("streams", []) if remote_config else []
        if not streams and config.camera_source and remote_config:
            logger.warning(
                "Sin streams configurados; usando CAMERA_SOURCE legacy (%s)",
                config.camera_source,
            )
            streams = [
                {
                    "id": "legacy",
                    "enabled": True,
                    "source_type": "rtsp"
                    if config.camera_source.startswith(("rtsp://", "rtsps://"))
                    else "usb",
                    "connection_config": {
                        "url": config.camera_source
                        if config.camera_source.startswith(("rtsp://", "rtsps://"))
                        else None,
                        "device": config.camera_source
                        if config.camera_source.startswith("/dev/")
                        else "/dev/video0",
                    },
                }
            ]

        if remote_config and is_within_schedule(remote_config) and streams:
            interval = interval_seconds(remote_config)
            img_cfg = remote_config.get("image_settings", {})
            for stream in streams:
                if not stream.get("enabled", True):
                    continue
                stream_id = stream.get("id", "")
                stream_key = stream_id or "default"
                last = last_capture.get(stream_key, 0.0)
                if now - last < interval:
                    continue
                source = resolve_stream_source(stream)
                if not source:
                    logger.warning("Stream %s sin fuente configurada", stream.get("name", stream_id))
                    continue
                ts = int(time.time())
                dest = PENDING_DIR / f"capture_{stream_key}_{ts}.jpg"
                try:
                    capture_image(
                        dest,
                        width=int(img_cfg.get("width", 1280)),
                        height=int(img_cfg.get("height", 720)),
                        jpeg_quality=int(img_cfg.get("jpeg_quality", 75)),
                        max_kb=int(img_cfg.get("max_kb", 500)),
                        mock=mock_camera,
                        source=source,
                    )
                    sid = None if stream_id == "legacy" else stream_id
                    queue.enqueue(str(dest), stream_id=sid)
                    last_capture[stream_key] = now
                    logger.info(
                        "Captura encolada: %s (stream %s)",
                        dest.name,
                        stream.get("name", stream_id),
                    )
                except Exception as exc:
                    logger.error(
                        "Fallo de captura stream %s: %s",
                        stream.get("name", stream_id),
                        exc,
                    )
        elif not remote_config:
            logger.debug("Sin configuración remota; reintentando...")

        time.sleep(2)


def test_camera(mock: bool) -> None:
    config = load_config(mock_camera=mock)
    dest = PENDING_DIR / "test.jpg"
    PENDING_DIR.mkdir(parents=True, exist_ok=True)
    source = config.camera_source or "/dev/video0"
    if config.camera_source and not mock:
        logger.info("Usando fuente: %s", config.camera_source)
    capture_image(dest, 640, 480, 75, 500, mock=mock, source=source)
    logger.info("Imagen de prueba guardada en %s (%s bytes)", dest, dest.stat().st_size)


def diagnose(mock_camera: bool = False) -> None:
    config = load_config(mock_camera=mock_camera)
    tok = config.device_token
    masked = (tok[:4] + "..." + tok[-4:]) if len(tok) > 8 else ("(vacío)" if not tok else "(corto)")

    print("=== Configuración ===")
    print(f"  BACKEND_URL   = {config.backend_url or '(vacío)'}")
    print(f"  DEVICE_ID     = {config.device_id or '(vacío)'}")
    print(f"  DEVICE_TOKEN  = {masked}")
    print(f"  CAMERA_SOURCE = {config.camera_source or '(streams remotos / webcam default)'}")
    print(f"  QUEUE_DB      = {config.queue_db}")

    if "localhost" in config.backend_url or "127.0.0.1" in config.backend_url:
        print("  ⚠️  BACKEND_URL apunta a localhost: en la Raspberry debe ser la IP/dominio del servidor.")

    ok = True
    if not config.device_id or not config.device_token:
        print("\n❌ Falta DEVICE_ID o DEVICE_TOKEN en .env")
        ok = False

    print("\n=== Conexión con el backend ===")
    uploader = Uploader(config)
    conn_ok, message = uploader.check_connection()
    print(f"  {'✅' if conn_ok else '❌'} {message}")
    ok = ok and conn_ok

    remote = uploader.fetch_config()
    if remote:
        streams = remote.get("streams", [])
        print(f"\n=== Streams remotos ({len(streams)}) ===")
        for s in streams:
            status = "activo" if s.get("enabled") else "inactivo"
            print(f"  - {s.get('name')} [{s.get('source_type')}] ({status})")
    else:
        print("\n=== Streams remotos ===")
        print("  (no se pudo obtener configuración)")

    print("\n=== Cola de subidas ===")
    queue = UploadQueue(config.queue_db)
    stats = queue.stats()
    if not stats:
        print("  (cola vacía: aún no se ha capturado/encolado ninguna imagen)")
    else:
        labels = {
            "pending": "pendientes",
            "uploading": "subiendo",
            "failed": "fallidas (con reintentos)",
            "acked": "confirmadas por el backend",
        }
        for status, count in sorted(stats.items()):
            print(f"  {labels.get(status, status):32} {count}")

    print()
    print("✅ Diagnóstico OK: el agente puede comunicarse con el backend." if ok
          else "❌ Hay problemas de configuración o conexión (ver arriba).")
    sys.exit(0 if ok else 1)


def main() -> None:
    parser = argparse.ArgumentParser(description="EPP Sentinel Agent")
    parser.add_argument("--mock-camera", action="store_true", help="Generar imagen sintética")
    parser.add_argument("--test-camera", action="store_true", help="Probar captura y salir")
    parser.add_argument("--diagnose", action="store_true", help="Verificar config, conexión y cola")
    args = parser.parse_args()
    if args.diagnose:
        diagnose(args.mock_camera)
        return
    if args.test_camera:
        test_camera(args.mock_camera)
        return
    run_loop(mock_camera=args.mock_camera)


if __name__ == "__main__":
    main()
