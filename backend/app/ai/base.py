from typing import Protocol


class AIProvider(Protocol):
    def analyze_image_sync(self, image_bytes: bytes, prompt: str, required_epp: list[str]) -> dict:
        ...
