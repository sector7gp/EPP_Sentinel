from __future__ import annotations

import csv
import io
import json
from datetime import datetime

from fastapi import APIRouter, Depends, Query
from fastapi.responses import StreamingResponse
from sqlalchemy.orm import Session, joinedload

from app.database import get_db
from app.models import Analysis, Capture, Device
from app.schemas.schemas import AnalysisListResponse, AnalysisResponse
from app.utils.auth import get_current_admin

router = APIRouter(prefix="/analyses", tags=["analyses"])


def _build_response(row: Analysis) -> AnalysisResponse:
    capture = row.capture
    device = capture.device
    epp = json.loads(row.result_json) if row.result_json else {}
    return AnalysisResponse(
        id=row.id,
        capture_id=row.capture_id,
        device_id=capture.device_id,
        device_name=device.name if device else "",
        image_url=f"/storage/{capture.image_path}",
        captured_at=capture.captured_at,
        analyzed_at=row.analyzed_at,
        provider=row.provider,
        cumple_normativa=row.cumple_normativa,
        observaciones=row.observaciones,
        epp_results={k: v for k, v in epp.items() if k not in ("cumple_normativa", "observaciones", "error")},
        status=row.status,
    )


@router.get("", response_model=AnalysisListResponse)
def list_analyses(
    db: Session = Depends(get_db),
    _: str = Depends(get_current_admin),
    page: int = Query(1, ge=1),
    page_size: int = Query(20, ge=1, le=100),
    cumple: bool | None = None,
    search: str | None = None,
    from_date: datetime | None = None,
    to_date: datetime | None = None,
):
    q = (
        db.query(Analysis)
        .join(Capture)
        .join(Device)
        .options(joinedload(Analysis.capture).joinedload(Capture.device))
        .order_by(Analysis.analyzed_at.desc())
    )
    if cumple is not None:
        q = q.filter(Analysis.cumple_normativa == cumple)
    if from_date:
        q = q.filter(Analysis.analyzed_at >= from_date)
    if to_date:
        q = q.filter(Analysis.analyzed_at <= to_date)
    if search:
        like = f"%{search}%"
        q = q.filter(
            (Analysis.observaciones.ilike(like))
            | (Device.name.ilike(like))
            | (Analysis.result_json.ilike(like))
        )
    total = q.count()
    rows = q.offset((page - 1) * page_size).limit(page_size).all()
    return AnalysisListResponse(
        items=[_build_response(r) for r in rows],
        total=total,
        page=page,
        page_size=page_size,
    )


@router.get("/export")
def export_analyses(
    db: Session = Depends(get_db),
    _: str = Depends(get_current_admin),
    cumple: bool | None = None,
    from_date: datetime | None = None,
    to_date: datetime | None = None,
):
    q = (
        db.query(Analysis)
        .join(Capture)
        .join(Device)
        .options(joinedload(Analysis.capture).joinedload(Capture.device))
        .order_by(Analysis.analyzed_at.desc())
    )
    if cumple is not None:
        q = q.filter(Analysis.cumple_normativa == cumple)
    if from_date:
        q = q.filter(Analysis.analyzed_at >= from_date)
    if to_date:
        q = q.filter(Analysis.analyzed_at <= to_date)

    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(
        [
            "id",
            "device",
            "captured_at",
            "analyzed_at",
            "provider",
            "cumple_normativa",
            "observaciones",
            "result_json",
        ]
    )
    for row in q.limit(10000).all():
        cap = row.capture
        writer.writerow(
            [
                row.id,
                cap.device.name if cap.device else "",
                cap.captured_at.isoformat(),
                row.analyzed_at.isoformat(),
                row.provider,
                row.cumple_normativa,
                row.observaciones or "",
                row.result_json,
            ]
        )
    output.seek(0)
    return StreamingResponse(
        iter([output.getvalue()]),
        media_type="text/csv",
        headers={"Content-Disposition": "attachment; filename=analyses_export.csv"},
    )
