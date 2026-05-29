import base64
import json
import re

from openai import OpenAI

from app.config import get_settings


class OpenAIProvider:
    def __init__(self, api_key: str, model: str):
        self.client = OpenAI(api_key=api_key, timeout=get_settings().ai_request_timeout)
        self.model = model

    def analyze_image_sync(self, image_bytes: bytes, prompt: str, required_epp: list[str]) -> dict:
        b64 = base64.standard_b64encode(image_bytes).decode()
        response = self.client.chat.completions.create(
            model=self.model,
            messages=[
                {
                    "role": "user",
                    "content": [
                        {"type": "text", "text": prompt},
                        {
                            "type": "image_url",
                            "image_url": {"url": f"data:image/jpeg;base64,{b64}"},
                        },
                    ],
                }
            ],
            response_format={"type": "json_object"},
            max_tokens=1024,
        )
        text = response.choices[0].message.content or "{}"
        return _parse_json(text)


def _parse_json(text: str) -> dict:
    text = text.strip()
    if text.startswith("```"):
        text = re.sub(r"^```(?:json)?\s*", "", text)
        text = re.sub(r"\s*```$", "", text)
    return json.loads(text)
