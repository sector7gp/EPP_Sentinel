from __future__ import annotations

import json
from pathlib import Path

from sqlalchemy.orm import Session

from app.config import get_settings
from app.constants import AuditEventType
from app.models import AISettings, Analysis, Capture, Device, OperatorProfile, VideoStream
from app.schemas.schemas import EPPAnalysisResult
from app.services.audit import log_event
from app.services.profile_service import ensure_default_profile
from app.ai.factory import get_ai_provider
from app.ai.prompt_builder import PromptBuilder


def get_required_epp_for_profile(db: Session, profile_id: str) -> list[str]:
    profile = db.get(OperatorProfile, profile_id)
    if not profile:
        return []
    return [r.epp_type for r in profile.epp_requirements]


def get_required_epp_for_stream(db: Session, stream: VideoStream | None) -> list[str]:
    if stream and stream.profile_id:
        return get_required_epp_for_profile(db, stream.profile_id)
    default = ensure_default_profile(db)
    return get_required_epp_for_profile(db, default.id)


def get_stream_profile_id(db: Session, stream: VideoStream | None) -> str:
    if stream and stream.profile_id:
        return stream.profile_id
    return ensure_default_profile(db).id


def compute_compliance(result: dict, required: list[str]) -> bool:
    for epp in required:
        if not result.get(epp, False):
            return False
    return len(required) > 0


def run_analysis(db: Session, capture: Capture, image_bytes: bytes) -> Analysis:
    device = db.get(Device, capture.device_id)
    if not device:
        raise ValueError("Device not found")

    stream = None
    if capture.stream_id:
        stream = db.get(VideoStream, capture.stream_id)

    required = get_required_epp_for_stream(db, stream)
    ai_settings = db.get(AISettings, 1)
    if not ai_settings:
        ai_settings = AISettings()
        db.add(ai_settings)
        db.commit()
        db.refresh(ai_settings)

    profile_id = get_stream_profile_id(db, stream)

    prompt = PromptBuilder.build(required, ai_settings.active_provider)
    provider = get_ai_provider(ai_settings)

    try:
        raw = provider.analyze_image_sync(image_bytes, prompt, required)
        validated = EPPAnalysisResult.model_validate(raw)
        result_dict = validated.to_result_dict()
        for epp in required:
            if epp not in result_dict or result_dict[epp] is None:
                result_dict[epp] = False
        cumple = compute_compliance(result_dict, required)
        result_dict["cumple_normativa"] = cumple

        analysis = Analysis(
            capture_id=capture.id,
            profile_id=profile_id,
            provider=ai_settings.active_provider,
            result_json=json.dumps(result_dict),
            cumple_normativa=cumple,
            observaciones=validated.observaciones or "",
            status="completed",
        )
        db.add(analysis)
        db.commit()
        db.refresh(analysis)

        log_event(
            db,
            AuditEventType.AI_RESPONSE.value,
            f"Análisis completado para captura {capture.id}",
            device_id=device.id,
            payload={"cumple_normativa": cumple, "provider": ai_settings.active_provider},
        )
        return analysis
    except Exception as exc:
        log_event(
            db,
            AuditEventType.AI_ERROR.value,
            f"Error en análisis IA: {exc}",
            device_id=device.id,
        )
        analysis = Analysis(
            capture_id=capture.id,
            profile_id=profile_id,
            provider=ai_settings.active_provider,
            result_json=json.dumps({"error": str(exc)}),
            cumple_normativa=False,
            observaciones=str(exc),
            status="failed",
        )
        db.add(analysis)
        db.commit()
        db.refresh(analysis)
        return analysis


def save_capture_image(device_id: str, capture_id: str, content: bytes) -> str:
    settings = get_settings()
    dest_dir = Path(settings.storage_path) / "captures" / device_id
    dest_dir.mkdir(parents=True, exist_ok=True)
    path = dest_dir / f"{capture_id}.jpg"
    path.write_bytes(content)
    return str(path.relative_to(settings.storage_path))
