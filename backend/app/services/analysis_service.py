import json
from pathlib import Path

from sqlalchemy.orm import Session, joinedload

from app.config import get_settings
from app.constants import AuditEventType
from app.models import AISettings, Analysis, Capture, Device, OperatorProfile
from app.schemas.schemas import EPPAnalysisResult
from app.services.audit import log_event
from app.services.device_service import ensure_default_profile
from app.ai.factory import get_ai_provider
from app.ai.prompt_builder import PromptBuilder


def get_required_epp(db: Session, device: Device) -> list[str]:
    profile_id = None
    if device.profile_assignment:
        profile_id = device.profile_assignment.profile_id
    if not profile_id:
        default = ensure_default_profile(db)
        profile_id = default.id
    profile = db.get(OperatorProfile, profile_id)
    if not profile:
        return []
    return [r.epp_type for r in profile.epp_requirements]


def compute_compliance(result: dict, required: list[str]) -> bool:
    for epp in required:
        if not result.get(epp, False):
            return False
    return len(required) > 0


def run_analysis(db: Session, capture: Capture, image_bytes: bytes) -> Analysis:
    device = db.get(Device, capture.device_id)
    if not device:
        raise ValueError("Device not found")

    required = get_required_epp(db, device)
    ai_settings = db.get(AISettings, 1)
    if not ai_settings:
        ai_settings = AISettings()
        db.add(ai_settings)
        db.commit()
        db.refresh(ai_settings)

    profile_id = device.profile_assignment.profile_id if device.profile_assignment else None
    if not profile_id:
        profile_id = ensure_default_profile(db).id

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
