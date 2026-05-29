import json
import re

import google.generativeai as genai

from app.config import get_settings


class GeminiProvider:
    def __init__(self, api_key: str, model: str):
        genai.configure(api_key=api_key)
        self.model = genai.GenerativeModel(model)

    def analyze_image_sync(self, image_bytes: bytes, prompt: str, required_epp: list[str]) -> dict:
        response = self.model.generate_content(
            [
                prompt,
                {"mime_type": "image/jpeg", "data": image_bytes},
            ],
            generation_config=genai.GenerationConfig(
                response_mime_type="application/json",
                max_output_tokens=1024,
            ),
            request_options={"timeout": get_settings().ai_request_timeout},
        )
        text = response.text or "{}"
        return _parse_json(text)


def _parse_json(text: str) -> dict:
    text = text.strip()
    if text.startswith("```"):
        text = re.sub(r"^```(?:json)?\s*", "", text)
        text = re.sub(r"\s*```$", "", text)
    return json.loads(text)
