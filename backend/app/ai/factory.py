from __future__ import annotations

from fastapi import HTTPException

from app.constants import AIProviderName
from app.models import AISettings
from app.utils.crypto import decrypt_value
from app.ai.openai_provider import OpenAIProvider
from app.ai.anthropic_provider import AnthropicProvider
from app.ai.gemini_provider import GeminiProvider


def get_api_key(settings: AISettings, provider: str) -> str | None:
    mapping = {
        AIProviderName.OPENAI.value: settings.openai_api_key_enc,
        AIProviderName.ANTHROPIC.value: settings.anthropic_api_key_enc,
        AIProviderName.GEMINI.value: settings.gemini_api_key_enc,
    }
    enc = mapping.get(provider)
    if not enc:
        return None
    return decrypt_value(enc)


def get_ai_provider(settings: AISettings):
    provider = settings.active_provider
    api_key = get_api_key(settings, provider)
    if not api_key:
        raise HTTPException(
            status_code=400,
            detail=f"API key no configurada para proveedor {provider}",
        )

    model = settings.model_name
    if provider == AIProviderName.OPENAI.value:
        return OpenAIProvider(api_key, model or "gpt-4o-mini")
    if provider == AIProviderName.ANTHROPIC.value:
        return AnthropicProvider(api_key, model or "claude-3-5-haiku-20241022")
    if provider == AIProviderName.GEMINI.value:
        return GeminiProvider(api_key, model or "gemini-1.5-flash")
    raise HTTPException(status_code=400, detail=f"Proveedor no soportado: {provider}")
