import time
from pathlib import Path

import httpx

from agent.config import AgentConfig


class Uploader:
    def __init__(self, config: AgentConfig):
        self.config = config

    def upload(self, file_path: Path) -> bool:
        url = f"{self.config.backend_url}/api/v1/captures"
        headers = {"X-Device-Token": self.config.device_token}
        with open(file_path, "rb") as f:
            files = {"file": (file_path.name, f, "image/jpeg")}
            with httpx.Client(timeout=120.0, verify=True) as client:
                response = client.post(url, headers=headers, files=files)
                response.raise_for_status()
                data = response.json()
                return data.get("capture_id") is not None

    def fetch_config(self) -> dict | None:
        url = f"{self.config.backend_url}/api/v1/devices/{self.config.device_id}/config"
        headers = {"X-Device-Token": self.config.device_token}
        try:
            with httpx.Client(timeout=30.0) as client:
                response = client.get(url, headers=headers)
                response.raise_for_status()
                return response.json()
        except httpx.HTTPError:
            return None


def retry_delay(retries: int) -> float:
    return min(300.0, 2 ** retries * 5)
