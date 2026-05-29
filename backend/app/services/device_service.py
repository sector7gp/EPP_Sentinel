import secrets
from datetime import datetime, time

from sqlalchemy.orm import Session

from app.models import (
    CaptureSchedule,
    Device,
    DeviceProfileAssignment,
    ImageSettings,
    OperatorProfile,
    ProfileEPPRequirement,
)


def create_device(db: Session, name: str) -> Device:
    device = Device(name=name, api_token=secrets.token_urlsafe(32))
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
    return device


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
    if device.profile_assignment:
        parts.append(f"p{device.profile_assignment.profile_id}")
    return "-".join(parts) or "default"


def ensure_default_profile(db: Session) -> OperatorProfile:
    profile = db.query(OperatorProfile).filter(OperatorProfile.name == "Operario de Planta").first()
    if profile:
        return profile
    profile = OperatorProfile(
        name="Operario de Planta",
        description="Perfil por defecto con EPP básicos de planta",
    )
    db.add(profile)
    db.flush()
    for epp in ("casco_seguridad", "chaleco_reflectivo", "calzado_seguridad", "guantes_seguridad"):
        db.add(ProfileEPPRequirement(profile_id=profile.id, epp_type=epp))
    db.commit()
    db.refresh(profile)
    return profile


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
