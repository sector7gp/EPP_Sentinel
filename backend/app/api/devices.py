from __future__ import annotations

from fastapi import APIRouter, Depends, Header, HTTPException
from sqlalchemy.orm import Session, joinedload

from app.constants import AuditEventType
from app.database import get_db
from app.models import CaptureSchedule, Device, ImageSettings, OperatorProfile, VideoStream
from app.schemas.schemas import (
    DeviceConfigResponse,
    DeviceCreate,
    DeviceResponse,
    DeviceUpdate,
    ImageSettingsResponse,
    ImageSettingsUpdate,
    ScheduleResponse,
    ScheduleUpdate,
    StreamConfigResponse,
    StreamConnectionConfig,
    StreamCreate,
    StreamResponse,
    StreamUpdate,
)
from app.services.audit import log_event
from app.services.device_service import (
    create_device,
    delete_device,
    device_is_online,
    get_device_config_version,
    touch_device,
    update_device,
)
from app.services.stream_service import (
    create_stream,
    delete_stream,
    get_stream_or_404,
    parse_connection_config,
    update_stream,
)
from app.utils.auth import get_current_admin, get_current_admin_optional, get_device_by_token

router = APIRouter(tags=["devices"])


def _connection_schema(raw: str) -> StreamConnectionConfig:
    data = parse_connection_config(raw)
    return StreamConnectionConfig(**data)


def _stream_response(stream: VideoStream) -> StreamResponse:
    profile = stream.profile
    return StreamResponse(
        id=stream.id,
        device_id=stream.device_id,
        name=stream.name,
        enabled=stream.enabled,
        source_type=stream.source_type,
        connection_config=_connection_schema(stream.connection_config),
        profile_id=stream.profile_id,
        profile_name=profile.name if profile else None,
        required_epp=[r.epp_type for r in profile.epp_requirements] if profile else [],
    )


def _device_response(device: Device) -> DeviceResponse:
    return DeviceResponse(
        id=device.id,
        name=device.name,
        location=device.location,
        api_token=device.api_token,
        last_seen_at=device.last_seen_at,
        online=device_is_online(device),
        stream_count=len(device.streams) if device.streams else 0,
    )


@router.get("/devices", response_model=list[DeviceResponse])
def list_devices(
    db: Session = Depends(get_db),
    _: str = Depends(get_current_admin),
):
    devices = (
        db.query(Device)
        .options(joinedload(Device.streams))
        .order_by(Device.created_at.desc())
        .all()
    )
    return [_device_response(d) for d in devices]


@router.post("/devices", response_model=DeviceResponse)
def register_device(
    body: DeviceCreate,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    device = create_device(db, body.name.strip(), body.location.strip() if body.location else None)
    device = _load_device(db, device.id)
    log_event(db, AuditEventType.CONFIG_CHANGE.value, f"Dispositivo registrado: {device.name}", user=admin)
    return _device_response(device)


@router.patch("/devices/{device_id}", response_model=DeviceResponse)
def patch_device(
    device_id: str,
    body: DeviceUpdate,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    device = _load_device(db, device_id)
    try:
        update_device(db, device, name=body.name, location=body.location)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    log_event(
        db,
        AuditEventType.CONFIG_CHANGE.value,
        f"Dispositivo actualizado: {device.name}",
        device_id=device_id,
        user=admin,
    )
    return _device_response(_load_device(db, device_id))


@router.delete("/devices/{device_id}")
def remove_device(
    device_id: str,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    device = _load_device(db, device_id)
    name = device.name
    delete_device(db, device)
    log_event(db, AuditEventType.CONFIG_CHANGE.value, f"Dispositivo eliminado: {name}", user=admin)
    return {"status": "ok"}


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
    streams = [
        StreamConfigResponse(
            id=s.id,
            name=s.name,
            enabled=s.enabled,
            source_type=s.source_type,
            connection_config=_connection_schema(s.connection_config),
            profile_id=s.profile_id,
        )
        for s in device.streams
        if s.enabled or x_device_token is None
    ]
    if x_device_token:
        streams = [s for s in streams if s.enabled]
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
        streams=streams,
        config_version=get_device_config_version(device),
    )


@router.get("/devices/{device_id}/streams", response_model=list[StreamResponse])
def list_streams(
    device_id: str,
    db: Session = Depends(get_db),
    _: str = Depends(get_current_admin),
):
    device = _load_device(db, device_id)
    return [_stream_response(s) for s in device.streams]


@router.post("/devices/{device_id}/streams", response_model=StreamResponse)
def add_stream(
    device_id: str,
    body: StreamCreate,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    device = _load_device(db, device_id)
    stream = create_stream(
        db,
        device,
        name=body.name,
        source_type=body.source_type,
        connection_config=body.connection_config.model_dump(exclude_none=True),
        profile_id=body.profile_id,
        enabled=body.enabled,
    )
    stream = get_stream_or_404(db, device_id, stream.id)
    log_event(
        db,
        AuditEventType.CONFIG_CHANGE.value,
        f"Stream creado: {stream.name}",
        device_id=device_id,
        user=admin,
    )
    return _stream_response(stream)


@router.put("/devices/{device_id}/streams/{stream_id}", response_model=StreamResponse)
def edit_stream(
    device_id: str,
    stream_id: str,
    body: StreamUpdate,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    stream = get_stream_or_404(db, device_id, stream_id)
    conn = body.connection_config.model_dump(exclude_none=True) if body.connection_config else None
    stream = update_stream(
        db,
        stream,
        name=body.name,
        source_type=body.source_type,
        connection_config=conn,
        profile_id=body.profile_id,
        enabled=body.enabled,
    )
    stream = get_stream_or_404(db, device_id, stream.id)
    log_event(
        db,
        AuditEventType.CONFIG_CHANGE.value,
        f"Stream actualizado: {stream.name}",
        device_id=device_id,
        user=admin,
    )
    return _stream_response(stream)


@router.delete("/devices/{device_id}/streams/{stream_id}")
def remove_stream(
    device_id: str,
    stream_id: str,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    stream = get_stream_or_404(db, device_id, stream_id)
    name = stream.name
    delete_stream(db, stream)
    log_event(
        db,
        AuditEventType.CONFIG_CHANGE.value,
        f"Stream eliminado: {name}",
        device_id=device_id,
        user=admin,
    )
    return {"status": "ok"}


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
    log_event(db, AuditEventType.CONFIG_CHANGE.value, "Imagen actualizada", device_id=device_id, user=admin)
    return ImageSettingsResponse(
        device_id=device_id,
        width=i.width,
        height=i.height,
        jpeg_quality=i.jpeg_quality,
        max_kb=i.max_kb,
    )


def _load_device(db: Session, device_id: str) -> Device:
    device = (
        db.query(Device)
        .options(
            joinedload(Device.schedule),
            joinedload(Device.image_settings),
            joinedload(Device.streams).joinedload(VideoStream.profile).joinedload(OperatorProfile.epp_requirements),
        )
        .filter(Device.id == device_id)
        .first()
    )
    if not device:
        raise HTTPException(status_code=404, detail="Dispositivo no encontrado")
    return device
