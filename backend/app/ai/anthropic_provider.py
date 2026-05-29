import base64
import json
import re

import anthropic

from app.config import get_settings


class AnthropicProvider:
    def __init__(self, api_key: str, model: str):
        self.client = anthropic.Anthropic(api_key=api_key, timeout=get_settings().ai_request_timeout)
        self.model = model

    def analyze_image_sync(self, image_bytes: bytes, prompt: str, required_epp: list[str]) -> dict:
        b64 = base64.standard_b64encode(image_bytes).decode()
        response = self.client.messages.create(
            model=self.model,
            max_tokens=1024,
            messages=[
                {
                    "role": "user",
                    "content": [
                        {
                            "type": "image",
                            "source": {
                                "type": "base64",
                                "media_type": "image/jpeg",
                                "data": b64,
                            },
                        },
                        {"type": "text", "text": prompt},
                    ],
                }
            ],
        )
        text = ""
        for block in response.content:
            if hasattr(block, "text"):
                text += block.text
        return _parse_json(text)


def _parse_json(text: str) -> dict:
    text = text.strip()
    match = re.search(r"\{.*\}", text, re.DOTALL)
    if match:
        text = match.group(0)
    return json.loads(text)
