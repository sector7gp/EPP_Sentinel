from __future__ import annotations

from fastapi import APIRouter, Depends, Query
from sqlalchemy.orm import Session

from app.database import get_db
from app.models import AuditLog
from app.schemas.schemas import AuditLogListResponse, AuditLogResponse
from app.utils.auth import get_current_admin

router = APIRouter(prefix="/audit-logs", tags=["audit-logs"])


@router.get("", response_model=AuditLogListResponse)
def list_audit_logs(
    db: Session = Depends(get_db),
    _: str = Depends(get_current_admin),
    event_type: str | None = None,
    limit: int = Query(100, ge=1, le=500),
    offset: int = Query(0, ge=0),
):
    q = db.query(AuditLog).order_by(AuditLog.created_at.desc())
    if event_type:
        q = q.filter(AuditLog.event_type == event_type)
    total = q.count()
    items = q.offset(offset).limit(limit).all()
    return AuditLogListResponse(
        items=[AuditLogResponse.model_validate(i) for i in items],
        total=total,
    )
