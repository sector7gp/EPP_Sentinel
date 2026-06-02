from __future__ import annotations

import secrets
from datetime import datetime, time

from sqlalchemy.orm import Session

from app.models import (
    Analysis,
    AuditLog,
    Capture,
    CaptureSchedule,
    Device,
    DeviceProfileAssignment,
    ImageSettings,
    VideoStream,
)
from app.services.stream_service import create_default_stream


def create_device(db: Session, name: str, location: str | None = None) -> Device:
    device = Device(name=name, location=location, api_token=secrets.token_urlsafe(32))
    db.add(device)
    db.flush()
    db.add(
        CaptureSchedule(
            device_id=device.id,
            start_time=time(7, 0),
            end_time=time(18, 0),
            interval_value=5,
            interval_unit="minutes",
            enabled_days="0,1,2,3,4,5,6",
        )
    )
    db.add(
        ImageSettings(
            device_id=device.id,
            width=1280,
            height=720,
            jpeg_quality=75,
            max_kb=500,
        )
    )
    db.commit()
    db.refresh(device)
    create_default_stream(db, device)
    return device


def update_device(db: Session, device: Device, *, name: str | None = None, location: str | None = None) -> Device:
    if name is not None:
        if not name.strip():
            raise ValueError("El nombre es obligatorio")
        device.name = name.strip()
    if location is not None:
        device.location = location.strip() or None
    db.commit()
    db.refresh(device)
    return device


def delete_device(db: Session, device: Device) -> None:
    device_id = device.id
    db.expire(device)

    capture_ids = [
        row[0]
        for row in db.query(Capture.id).filter(Capture.device_id == device_id).all()
    ]
    if capture_ids:
        db.query(Analysis).filter(Analysis.capture_id.in_(capture_ids)).delete(
            synchronize_session=False
        )
    db.query(Capture).filter(Capture.device_id == device_id).delete(synchronize_session=False)
    db.query(VideoStream).filter(VideoStream.device_id == device_id).delete(synchronize_session=False)
    db.query(DeviceProfileAssignment).filter(DeviceProfileAssignment.device_id == device_id).delete(
        synchronize_session=False
    )
    db.query(CaptureSchedule).filter(CaptureSchedule.device_id == device_id).delete(synchronize_session=False)
    db.query(ImageSettings).filter(ImageSettings.device_id == device_id).delete(synchronize_session=False)
    db.query(AuditLog).filter(AuditLog.device_id == device_id).update(
        {AuditLog.device_id: None},
        synchronize_session=False,
    )
    db.query(Device).filter(Device.id == device_id).delete(synchronize_session=False)
    db.commit()


def device_is_online(device: Device, threshold_seconds: int = 300) -> bool:
    if not device.last_seen_at:
        return False
    delta = datetime.utcnow() - device.last_seen_at
    return delta.total_seconds() <= threshold_seconds


def touch_device(db: Session, device: Device) -> None:
    device.last_seen_at = datetime.utcnow()
    db.commit()


def get_device_config_version(device: Device) -> str:
    parts = []
    if device.schedule:
        s = device.schedule
        parts.append(f"s{s.start_time}-{s.end_time}-{s.interval_value}{s.interval_unit}-{s.enabled_days}")
    if device.image_settings:
        i = device.image_settings
        parts.append(f"i{i.width}x{i.height}-q{i.jpeg_quality}-m{i.max_kb}")
    for stream in sorted(device.streams or [], key=lambda x: x.id):
        cfg = stream.connection_config
        parts.append(
            f"st{stream.id}-{stream.enabled}-{stream.source_type}-{stream.profile_id}-{cfg}"
        )
    return "-".join(parts) or "default"


def assign_profile_to_device(db: Session, device_id: str, profile_id: str) -> DeviceProfileAssignment:
    assignment = db.query(DeviceProfileAssignment).filter_by(device_id=device_id).first()
    if assignment:
        assignment.profile_id = profile_id
    else:
        assignment = DeviceProfileAssignment(device_id=device_id, profile_id=profile_id)
        db.add(assignment)
    db.commit()
    db.refresh(assignment)
    return assignment
