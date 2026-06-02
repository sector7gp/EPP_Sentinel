import io
import subprocess
import tempfile
from pathlib import Path

from PIL import Image


def capture_image(
    output_path: Path,
    width: int,
    height: int,
    jpeg_quality: int,
    max_kb: int,
    mock: bool = False,
    source: str = "",
) -> Path:
    raw_path = output_path.with_suffix(".raw.jpg")
    if mock:
        _generate_mock_image(raw_path, width, height)
    elif source.startswith(("rtsp://", "rtsps://")):
        _capture_from_rtsp(raw_path, source, width, height)
    elif source.startswith("/dev/"):
        _capture_from_usb(raw_path, source, width, height)
    else:
        _capture_from_camera(raw_path, width, height)
    return compress_image(raw_path, output_path, jpeg_quality, max_kb)


def _capture_from_rtsp(path: Path, url: str, width: int, height: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        "ffmpeg",
        "-nostdin",
        "-rtsp_transport", "tcp",
        "-y",
        "-i", url,
        "-frames:v", "1",
        "-vf", f"scale={width}:{height}",
        "-q:v", "2",
        "-an",
        str(path),
    ]
    try:
        subprocess.run(cmd, check=True, capture_output=True, timeout=30)
    except FileNotFoundError as exc:
        raise RuntimeError("ffmpeg no está instalado (sudo apt install -y ffmpeg)") from exc
    except subprocess.CalledProcessError as exc:
        stderr = exc.stderr.decode(errors="replace") if exc.stderr else ""
        raise RuntimeError(f"No se pudo capturar del stream RTSP: {stderr.strip()[-300:]}") from exc
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError("Timeout al conectar con el stream RTSP") from exc
    if not (path.exists() and path.stat().st_size > 0):
        raise RuntimeError("El stream RTSP no devolvió ningún frame")


def _capture_from_usb(path: Path, device: str, width: int, height: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    for cmd in (
        ["fswebcam", "-d", device, "-r", f"{width}x{height}", "--no-banner", "-S", "2", str(path)],
        ["fswebcam", "-d", device, "--no-banner", "-S", "2", str(path)],
    ):
        try:
            subprocess.run(cmd, check=True, capture_output=True, timeout=30)
            if path.exists() and path.stat().st_size > 0:
                return
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
    raise RuntimeError(f"No se pudo capturar desde {device} con fswebcam")


def _capture_from_camera(path: Path, width: int, height: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    for cmd in (
        ["fswebcam", "-r", f"{width}x{height}", "--no-banner", "-S", "2", str(path)],
        ["libcamera-still", "-o", str(path), "--width", str(width), "--height", str(height)],
    ):
        try:
            subprocess.run(cmd, check=True, capture_output=True, timeout=30)
            if path.exists() and path.stat().st_size > 0:
                return
        except (FileNotFoundError, subprocess.CalledProcessError):
            continue
    raise RuntimeError("No se pudo capturar imagen con fswebcam ni libcamera-still")


def _generate_mock_image(path: Path, width: int, height: int) -> None:
    img = Image.new("RGB", (width, height), color=(80, 120, 160))
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path, format="JPEG", quality=85)


def compress_image(src: Path, dest: Path, quality: int, max_kb: int) -> Path:
    img = Image.open(src).convert("RGB")
    img.thumbnail((img.width, img.height))
    quality = max(30, min(quality, 95))
    for q in range(quality, 29, -5):
        buffer = io.BytesIO()
        img.save(buffer, format="JPEG", quality=q, optimize=True)
        if buffer.tell() <= max_kb * 1024 or q <= 35:
            dest.write_bytes(buffer.getvalue())
            break
    if src != dest and src.exists():
        src.unlink(missing_ok=True)
    return dest
