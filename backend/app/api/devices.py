from __future__ import annotations

from fastapi import APIRouter, Depends, Header, HTTPException
from sqlalchemy.orm import Session, joinedload

from app.database import get_db
from app.models import CaptureSchedule, Device, ImageSettings
from app.schemas.schemas import (
    DeviceConfigResponse,
    DeviceCreate,
    DeviceResponse,
    ImageSettingsResponse,
    ProfileAssignRequest,
    ScheduleResponse,
    ScheduleUpdate,
    ImageSettingsUpdate,
)
from app.services.device_service import (
    assign_profile_to_device,
    create_device,
    get_device_config_version,
    touch_device,
)
from app.utils.auth import get_current_admin, get_current_admin_optional, get_device_by_token

router = APIRouter(tags=["devices"])


@router.get("/devices", response_model=list[DeviceResponse])
def list_devices(
    db: Session = Depends(get_db),
    _: str = Depends(get_current_admin),
):
    return db.query(Device).order_by(Device.created_at.desc()).all()


@router.post("/devices", response_model=DeviceResponse)
def register_device(
    body: DeviceCreate,
    db: Session = Depends(get_db),
    _: str = Depends(get_current_admin),
):
    return create_device(db, body.name)


@router.get("/devices/{device_id}/config", response_model=DeviceConfigResponse)
def get_device_config(
    device_id: str,
    db: Session = Depends(get_db),
    x_device_token: str | None = Header(default=None, alias="X-Device-Token"),
    admin: str | None = Depends(get_current_admin_optional),
):
    if x_device_token:
        device = get_device_by_token(db, x_device_token)
        if device.id != device_id:
            raise HTTPException(status_code=403, detail="Token no corresponde al dispositivo")
    elif admin:
        device = _load_device(db, device_id)
    else:
        raise HTTPException(status_code=401, detail="Autenticación requerida")
    device = _load_device(db, device.id)
    touch_device(db, device)
    schedule = device.schedule or CaptureSchedule(device_id=device.id)
    image = device.image_settings or ImageSettings(device_id=device.id)
    profile_id = device.profile_assignment.profile_id if device.profile_assignment else None
    return DeviceConfigResponse(
        device_id=device.id,
        schedule=ScheduleResponse(
            device_id=device.id,
            start_time=schedule.start_time,
            end_time=schedule.end_time,
            interval_value=schedule.interval_value,
            interval_unit=schedule.interval_unit,
            enabled_days=schedule.enabled_days,
        ),
        image_settings=ImageSettingsResponse(
            device_id=device.id,
            width=image.width,
            height=image.height,
            jpeg_quality=image.jpeg_quality,
            max_kb=image.max_kb,
        ),
        profile_id=profile_id,
        config_version=get_device_config_version(device),
    )


@router.put("/devices/{device_id}/schedule", response_model=ScheduleResponse)
def update_schedule(
    device_id: str,
    body: ScheduleUpdate,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    device = db.get(Device, device_id)
    if not device or not device.schedule:
        raise HTTPException(status_code=404, detail="Dispositivo no encontrado")
    s = device.schedule
    s.start_time = body.start_time
    s.end_time = body.end_time
    s.interval_value = body.interval_value
    s.interval_unit = body.interval_unit
    s.enabled_days = body.enabled_days
    db.commit()
    db.refresh(s)
    from app.services.audit import log_event
    from app.constants import AuditEventType

    log_event(db, AuditEventType.CONFIG_CHANGE.value, "Horario actualizado", device_id=device_id, user=admin)
    return ScheduleResponse(
        device_id=device_id,
        start_time=s.start_time,
        end_time=s.end_time,
        interval_value=s.interval_value,
        interval_unit=s.interval_unit,
        enabled_days=s.enabled_days,
    )


@router.put("/devices/{device_id}/image-settings", response_model=ImageSettingsResponse)
def update_image_settings(
    device_id: str,
    body: ImageSettingsUpdate,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    device = db.get(Device, device_id)
    if not device or not device.image_settings:
        raise HTTPException(status_code=404, detail="Dispositivo no encontrado")
    i = device.image_settings
    i.width = body.width
    i.height = body.height
    i.jpeg_quality = body.jpeg_quality
    i.max_kb = body.max_kb
    db.commit()
    db.refresh(i)
    from app.services.audit import log_event
    from app.constants import AuditEventType

    log_event(db, AuditEventType.CONFIG_CHANGE.value, "Imagen actualizada", device_id=device_id, user=admin)
    return ImageSettingsResponse(
        device_id=device_id,
        width=i.width,
        height=i.height,
        jpeg_quality=i.jpeg_quality,
        max_kb=i.max_kb,
    )


@router.put("/devices/{device_id}/profile")
def assign_profile(
    device_id: str,
    body: ProfileAssignRequest,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    device = db.get(Device, device_id)
    if not device:
        raise HTTPException(status_code=404, detail="Dispositivo no encontrado")
    assign_profile_to_device(db, device_id, body.profile_id)
    from app.services.audit import log_event
    from app.constants import AuditEventType

    log_event(db, AuditEventType.CONFIG_CHANGE.value, "Perfil asignado", device_id=device_id, user=admin)
    return {"status": "ok"}


def _load_device(db: Session, device_id: str) -> Device:
    device = (
        db.query(Device)
        .options(
            joinedload(Device.schedule),
            joinedload(Device.image_settings),
            joinedload(Device.profile_assignment),
        )
        .filter(Device.id == device_id)
        .first()
    )
    if not device:
        raise HTTPException(status_code=404, detail="Dispositivo no encontrado")
    return device
