from __future__ import annotations

import json
from typing import Any

from fastapi import HTTPException
from sqlalchemy.orm import Session, joinedload

from app.models import Capture, Device, OperatorProfile, VideoStream
from app.services.profile_service import ensure_default_profile

MAX_STREAMS_PER_DEVICE = 4
VALID_SOURCE_TYPES = frozenset({"usb", "rtsp"})


def parse_connection_config(raw: str) -> dict[str, Any]:
    try:
        data = json.loads(raw) if raw else {}
    except json.JSONDecodeError as exc:
        raise HTTPException(status_code=400, detail="connection_config JSON inválido") from exc
    if not isinstance(data, dict):
        raise HTTPException(status_code=400, detail="connection_config debe ser un objeto JSON")
    return data


def validate_stream_config(source_type: str, connection_config: dict[str, Any]) -> dict[str, Any]:
    if source_type not in VALID_SOURCE_TYPES:
        raise HTTPException(
            status_code=400,
            detail=f"source_type inválido: {source_type}. Use usb o rtsp",
        )
    if source_type == "usb":
        device = connection_config.get("device", "/dev/video0")
        if not isinstance(device, str) or not device.strip():
            raise HTTPException(status_code=400, detail="USB requiere device (ej. /dev/video0)")
        return {"device": device.strip()}
    url = connection_config.get("url", "")
    if not isinstance(url, str) or not url.strip().lower().startswith(("rtsp://", "rtsps://")):
        raise HTTPException(status_code=400, detail="RTSP requiere url válida (rtsp://...)")
    return {"url": url.strip()}


def ensure_profile_exists(db: Session, profile_id: str) -> OperatorProfile:
    profile = db.get(OperatorProfile, profile_id)
    if not profile:
        raise HTTPException(status_code=400, detail="Perfil EPP no encontrado")
    return profile


def count_device_streams(db: Session, device_id: str) -> int:
    return db.query(VideoStream).filter_by(device_id=device_id).count()


def create_stream(
    db: Session,
    device: Device,
    *,
    name: str,
    source_type: str,
    connection_config: dict[str, Any],
    profile_id: str,
    enabled: bool = True,
) -> VideoStream:
    if count_device_streams(db, device.id) >= MAX_STREAMS_PER_DEVICE:
        raise HTTPException(
            status_code=400,
            detail=f"Máximo {MAX_STREAMS_PER_DEVICE} streams por dispositivo",
        )
    ensure_profile_exists(db, profile_id)
    normalized = validate_stream_config(source_type, connection_config)
    stream = VideoStream(
        device_id=device.id,
        name=name.strip(),
        enabled=enabled,
        source_type=source_type,
        connection_config=json.dumps(normalized),
        profile_id=profile_id,
    )
    db.add(stream)
    db.commit()
    db.refresh(stream)
    return stream


def update_stream(
    db: Session,
    stream: VideoStream,
    *,
    name: str | None = None,
    source_type: str | None = None,
    connection_config: dict[str, Any] | None = None,
    profile_id: str | None = None,
    enabled: bool | None = None,
) -> VideoStream:
    new_type = source_type or stream.source_type
    if connection_config is not None:
        normalized = validate_stream_config(new_type, connection_config)
        stream.connection_config = json.dumps(normalized)
    elif source_type is not None and source_type != stream.source_type:
        existing = parse_connection_config(stream.connection_config)
        normalized = validate_stream_config(new_type, existing)
        stream.connection_config = json.dumps(normalized)

    if name is not None:
        if not name.strip():
            raise HTTPException(status_code=400, detail="El nombre del stream es obligatorio")
        stream.name = name.strip()
    if source_type is not None:
        stream.source_type = source_type
    if profile_id is not None:
        ensure_profile_exists(db, profile_id)
        stream.profile_id = profile_id
    if enabled is not None:
        stream.enabled = enabled

    db.commit()
    db.refresh(stream)
    return stream


def delete_stream(db: Session, stream: VideoStream) -> None:
    db.query(Capture).filter(Capture.stream_id == stream.id).update(
        {Capture.stream_id: None},
        synchronize_session=False,
    )
    db.delete(stream)
    db.commit()


def get_stream_or_404(db: Session, device_id: str, stream_id: str) -> VideoStream:
    stream = (
        db.query(VideoStream)
        .options(joinedload(VideoStream.profile).joinedload(OperatorProfile.epp_requirements))
        .filter_by(id=stream_id, device_id=device_id)
        .first()
    )
    if not stream:
        raise HTTPException(status_code=404, detail="Stream no encontrado")
    return stream


def create_default_stream(db: Session, device: Device) -> VideoStream:
    profile = ensure_default_profile(db)
    return create_stream(
        db,
        device,
        name="Cámara principal",
        source_type="usb",
        connection_config={"device": "/dev/video0"},
        profile_id=profile.id,
        enabled=True,
    )


def resolve_capture_source(stream: VideoStream) -> str:
    config = parse_connection_config(stream.connection_config)
    if stream.source_type == "rtsp":
        return config.get("url", "")
    return config.get("device", "/dev/video0")
