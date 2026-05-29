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
logger = logging.getLogger("epp-agent")

PENDING_DIR = Path(__file__).parent / "pending"


def process_queue(queue: UploadQueue, uploader: Uploader) -> None:
    for item_id, file_path, retries in queue.pending_items():
        path = Path(file_path)
        if not path.exists():
            queue.mark_acked(item_id)
            continue
        if retries > 0:
            time.sleep(retry_delay(retries))
        queue.mark_uploading(item_id)
        try:
            if uploader.upload(path):
                queue.mark_acked(item_id)
                path.unlink(missing_ok=True)
                logger.info("Subida exitosa: %s", path.name)
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
    last_capture = 0.0

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

        if remote_config and is_within_schedule(remote_config):
            interval = interval_seconds(remote_config)
            if now - last_capture >= interval:
                img_cfg = remote_config.get("image_settings", {})
                ts = int(time.time())
                dest = PENDING_DIR / f"capture_{ts}.jpg"
                try:
                    capture_image(
                        dest,
                        width=int(img_cfg.get("width", 1280)),
                        height=int(img_cfg.get("height", 720)),
                        jpeg_quality=int(img_cfg.get("jpeg_quality", 75)),
                        max_kb=int(img_cfg.get("max_kb", 500)),
                        mock=mock_camera,
                    )
                    queue.enqueue(str(dest))
                    last_capture = now
                    logger.info("Captura encolada: %s", dest.name)
                except Exception as exc:
                    logger.error("Fallo de captura: %s", exc)
        else:
            if not remote_config:
                logger.debug("Sin configuración remota; reintentando...")

        time.sleep(2)


def test_camera(mock: bool) -> None:
    dest = PENDING_DIR / "test.jpg"
    PENDING_DIR.mkdir(parents=True, exist_ok=True)
    capture_image(dest, 640, 480, 75, 500, mock=mock)
    logger.info("Imagen de prueba guardada en %s (%s bytes)", dest, dest.stat().st_size)


def main() -> None:
    parser = argparse.ArgumentParser(description="EPP Sentinel Agent")
    parser.add_argument("--mock-camera", action="store_true", help="Generar imagen sintética")
    parser.add_argument("--test-camera", action="store_true", help="Probar captura y salir")
    args = parser.parse_args()
    if args.test_camera:
        test_camera(args.mock_camera)
        return
    run_loop(mock_camera=args.mock_camera)


if __name__ == "__main__":
    main()
