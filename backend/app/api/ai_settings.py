from __future__ import annotations

from datetime import datetime

from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session

from app.constants import AuditEventType
from app.database import get_db
from app.models import AISettings
from app.schemas.schemas import AISettingsResponse, AISettingsUpdate
from app.services.audit import log_event
from app.utils.auth import get_current_admin
from app.utils.crypto import decrypt_value, encrypt_value, mask_secret

router = APIRouter(prefix="/ai-settings", tags=["ai-settings"])


def _get_or_create(db: Session) -> AISettings:
    settings = db.get(AISettings, 1)
    if not settings:
        settings = AISettings()
        db.add(settings)
        db.commit()
        db.refresh(settings)
    return settings


def _to_response(settings: AISettings) -> AISettingsResponse:
    def masked(enc: str | None) -> str | None:
        if not enc:
            return None
        try:
            return mask_secret(decrypt_value(enc))
        except Exception:
            return "••••"

    return AISettingsResponse(
        active_provider=settings.active_provider,
        model_name=settings.model_name,
        openai_api_key_set=bool(settings.openai_api_key_enc),
        anthropic_api_key_set=bool(settings.anthropic_api_key_enc),
        gemini_api_key_set=bool(settings.gemini_api_key_enc),
        openai_api_key_masked=masked(settings.openai_api_key_enc),
        anthropic_api_key_masked=masked(settings.anthropic_api_key_enc),
        gemini_api_key_masked=masked(settings.gemini_api_key_enc),
    )


@router.get("", response_model=AISettingsResponse)
def get_ai_settings(db: Session = Depends(get_db), _: str = Depends(get_current_admin)):
    return _to_response(_get_or_create(db))


@router.put("", response_model=AISettingsResponse)
def update_ai_settings(
    body: AISettingsUpdate,
    db: Session = Depends(get_db),
    admin: str = Depends(get_current_admin),
):
    settings = _get_or_create(db)
    if body.active_provider is not None:
        settings.active_provider = body.active_provider
    if body.model_name is not None:
        settings.model_name = body.model_name
    if body.openai_api_key:
        settings.openai_api_key_enc = encrypt_value(body.openai_api_key)
    if body.anthropic_api_key:
        settings.anthropic_api_key_enc = encrypt_value(body.anthropic_api_key)
    if body.gemini_api_key:
        settings.gemini_api_key_enc = encrypt_value(body.gemini_api_key)
    settings.updated_at = datetime.utcnow()
    db.commit()
    db.refresh(settings)
    log_event(db, AuditEventType.CONFIG_CHANGE.value, "Configuración IA actualizada", user=admin)
    return _to_response(settings)
