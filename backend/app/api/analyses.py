from __future__ import annotations

import csv
import io
import json
from datetime import datetime

from fastapi import APIRouter, Depends, Query
from fastapi.responses import StreamingResponse
from sqlalchemy.orm import Session, joinedload

from app.database import get_db
from app.models import Analysis, Capture, Device, OperatorProfile, VideoStream
from app.schemas.schemas import AnalysisListResponse, AnalysisResponse, StreamDashboardItem
from app.services.analysis_service import get_required_epp_for_profile
from app.services.device_service import device_is_online
from app.utils.auth import get_current_admin

router = APIRouter(prefix="/analyses", tags=["analyses"])


def _filter_epp_results(epp: dict, required: list[str]) -> dict[str, bool]:
    filtered: dict[str, bool] = {}
    for key in required:
        val = epp.get(key)
        if isinstance(val, bool):
            filtered[key] = val
    return filtered


def _required_epp_for_analysis(db: Session, row: Analysis, capture: Capture) -> list[str]:
    if row.profile_id:
        return get_required_epp_for_profile(db, row.profile_id)
    if capture.stream and capture.stream.profile_id:
        return get_required_epp_for_profile(db, capture.stream.profile_id)
    return []


def _build_response(db: Session, row: Analysis) -> AnalysisResponse:
    capture = row.capture
    device = capture.device
    stream = capture.stream
    epp = json.loads(row.result_json) if row.result_json else {}
    required = _required_epp_for_analysis(db, row, capture)
    if not required and row.profile_id:
        required = get_required_epp_for_profile(db, row.profile_id)
    return AnalysisResponse(
        id=row.id,
        capture_id=row.capture_id,
        device_id=capture.device_id,
        device_name=device.name if device else "",
        stream_id=capture.stream_id,
        stream_name=stream.name if stream else None,
        image_url=f"/storage/{capture.image_path}",
        captured_at=capture.captured_at,
        analyzed_at=row.analyzed_at,
        provider=row.provider,
        cumple_normativa=row.cumple_normativa,
        observaciones=row.observaciones,
        epp_results=_filter_epp_results(epp, required) if required else {
            k: v for k, v in epp.items() if k not in ("cumple_normativa", "observaciones", "error") and isinstance(v, bool)
        },
        required_epp=required,
        status=row.status,
    )


@router.get("/dashboard/streams", response_model=list[StreamDashboardItem])
def dashboard_streams(
    db: Session = Depends(get_db),
    _: str = Depends(get_current_admin),
):
    streams = (
        db.query(VideoStream)
        .join(Device)
        .options(
            joinedload(VideoStream.device),
            joinedload(VideoStream.profile).joinedload(OperatorProfile.epp_requirements),
        )
        .order_by(Device.name, VideoStream.name)
        .all()
    )
    items: list[StreamDashboardItem] = []
    for stream in streams:
        profile = stream.profile
        required = [r.epp_type for r in profile.epp_requirements] if profile else []
        latest_row = (
            db.query(Analysis)
            .join(Capture)
            .filter(Capture.stream_id == stream.id)
            .options(
                joinedload(Analysis.capture).joinedload(Capture.device),
                joinedload(Analysis.capture).joinedload(Capture.stream),
            )
            .order_by(Analysis.analyzed_at.desc())
            .first()
        )
        latest = _build_response(db, latest_row) if latest_row else None
        device = stream.device
        items.append(
            StreamDashboardItem(
                stream_id=stream.id,
                stream_name=stream.name,
                device_id=device.id,
                device_name=device.name,
                device_location=device.location,
                device_online=device_is_online(device),
                profile_id=stream.profile_id,
                profile_name=profile.name if profile else "",
                required_epp=required,
                latest_analysis=latest,
            )
        )
    return items


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
    stream_id: str | None = None,
):
    q = (
        db.query(Analysis)
        .join(Capture)
        .join(Device)
        .options(
            joinedload(Analysis.capture).joinedload(Capture.device),
            joinedload(Analysis.capture).joinedload(Capture.stream),
        )
        .order_by(Analysis.analyzed_at.desc())
    )
    if cumple is not None:
        q = q.filter(Analysis.cumple_normativa == cumple)
    if from_date:
        q = q.filter(Analysis.analyzed_at >= from_date)
    if to_date:
        q = q.filter(Analysis.analyzed_at <= to_date)
    if stream_id:
        q = q.filter(Capture.stream_id == stream_id)
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
        items=[_build_response(db, r) for r in rows],
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
            "stream_id",
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
                cap.stream_id or "",
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
