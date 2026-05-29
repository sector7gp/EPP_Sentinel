from functools import lru_cache

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str = "sqlite:///./epp_sentinel.db"
    secret_key: str = "dev-secret-change-me"
    admin_username: str = "admin"
    admin_password: str = "admin"
    cors_origins: str = "http://localhost:5173"
    storage_path: str = "./storage"
    access_token_expire_minutes: int = 480
    ai_request_timeout: int = 120


@lru_cache
def get_settings() -> Settings:
    return Settings()
