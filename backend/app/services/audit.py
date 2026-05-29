from __future__ import annotations

import json

from sqlalchemy.orm import Session

from app.models import AuditLog


def log_event(
    db: Session,
    event_type: str,
    message: str,
    *,
    device_id: str | None = None,
    user: str | None = None,
    payload: dict | None = None,
) -> AuditLog:
    entry = AuditLog(
        event_type=event_type,
        message=message,
        device_id=device_id,
        user=user,
        payload_json=json.dumps(payload) if payload else None,
    )
    db.add(entry)
    db.commit()
    db.refresh(entry)
    return entry
