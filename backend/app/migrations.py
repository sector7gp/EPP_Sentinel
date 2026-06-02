from pathlib import Path

from alembic import command
from alembic.config import Config


def run_migrations() -> None:
    """Apply pending Alembic migrations (required after schema updates)."""
    ini_path = Path(__file__).resolve().parent.parent / "alembic.ini"
    cfg = Config(str(ini_path))
    command.upgrade(cfg, "head")
