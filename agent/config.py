import os
from dataclasses import dataclass

from dotenv import load_dotenv

load_dotenv()


@dataclass
class AgentConfig:
    backend_url: str
    device_id: str
    device_token: str
    config_poll_seconds: int
    queue_db: str
    mock_camera: bool = False


def load_config(mock_camera: bool = False) -> AgentConfig:
    return AgentConfig(
        backend_url=os.getenv("BACKEND_URL", "http://localhost:8000").rstrip("/"),
        device_id=os.getenv("DEVICE_ID", ""),
        device_token=os.getenv("DEVICE_TOKEN", ""),
        config_poll_seconds=int(os.getenv("CONFIG_POLL_SECONDS", "300")),
        queue_db=os.getenv("QUEUE_DB", "./queue.db"),
        mock_camera=mock_camera,
    )
