import logging
import time
from pathlib import Path

import httpx

from agent.config import AgentConfig

logger = logging.getLogger("epp-agent")


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
        except httpx.HTTPStatusError as exc:
            logger.warning(
                "Config rechazada por el backend (HTTP %s) en %s",
                exc.response.status_code,
                url,
            )
            return None
        except httpx.HTTPError as exc:
            logger.warning("No se pudo conectar con el backend (%s): %s", url, exc)
            return None


    def check_connection(self) -> tuple[bool, str]:
        """Probar autenticación contra el backend. Devuelve (ok, mensaje)."""
        url = f"{self.config.backend_url}/api/v1/devices/{self.config.device_id}/config"
        headers = {"X-Device-Token": self.config.device_token}
        try:
            with httpx.Client(timeout=15.0) as client:
                response = client.get(url, headers=headers)
        except httpx.ConnectError as exc:
            return False, f"No se pudo conectar a {self.config.backend_url} ({exc})"
        except httpx.TimeoutException:
            return False, f"Timeout al conectar a {self.config.backend_url}"
        except httpx.HTTPError as exc:
            return False, f"Error de red: {exc}"
        if response.status_code == 200:
            return True, "Backend accesible y token válido"
        if response.status_code in (401, 403):
            return False, f"Token o DEVICE_ID inválido (HTTP {response.status_code})"
        if response.status_code == 404:
            return False, f"Dispositivo no encontrado (HTTP 404): revise DEVICE_ID"
        return False, f"Respuesta inesperada del backend (HTTP {response.status_code})"


def retry_delay(retries: int) -> float:
    return min(300.0, 2 ** retries * 5)
