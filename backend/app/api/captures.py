from datetime import datetime

from fastapi import APIRouter, Depends, File, Header, HTTPException, UploadFile
from sqlalchemy.orm import Session

from app.constants import AuditEventType
from app.database import get_db
from app.models import Capture
from app.schemas.schemas import CaptureUploadResponse
from app.services.analysis_service import run_analysis, save_capture_image
from app.services.audit import log_event
from app.services.device_service import touch_device
from app.utils.auth import get_device_by_token

router = APIRouter(tags=["captures"])


@router.post("/captures", response_model=CaptureUploadResponse)
async def upload_capture(
    file: UploadFile = File(...),
    db: Session = Depends(get_db),
    x_device_token: str = Header(..., alias="X-Device-Token"),
):
    device = get_device_by_token(db, x_device_token)
    touch_device(db, device)

    if not file.content_type or not file.content_type.startswith("image/"):
        log_event(
            db,
            AuditEventType.CAPTURE_FAILED.value,
            "Tipo de archivo inválido",
            device_id=device.id,
        )
        raise HTTPException(status_code=400, detail="Se requiere una imagen")

    content = await file.read()
    if len(content) < 100:
        log_event(db, AuditEventType.CAPTURE_FAILED.value, "Imagen vacía", device_id=device.id)
        raise HTTPException(status_code=400, detail="Imagen vacía")

    capture = Capture(
        device_id=device.id,
        image_path="pending",
        captured_at=datetime.utcnow(),
        upload_status="received",
        file_size_kb=len(content) // 1024,
    )
    db.add(capture)
    db.flush()

    path = save_capture_image(device.id, capture.id, content)
    capture.image_path = path
    db.commit()
    db.refresh(capture)

    log_event(
        db,
        AuditEventType.UPLOAD_SUCCESS.value,
        f"Captura recibida {capture.id}",
        device_id=device.id,
        payload={"capture_id": capture.id, "size_kb": capture.file_size_kb},
    )

    analysis = run_analysis(db, capture, content)

    return CaptureUploadResponse(
        capture_id=capture.id,
        status="processed",
        analysis_id=analysis.id if analysis else None,
    )
