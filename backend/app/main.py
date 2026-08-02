from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles

from app.config import get_settings
from app.database import SessionLocal
from app.migrations import run_migrations
from app.api import auth, devices, captures, profiles, ai_settings, analyses, audit_logs
from app.models import AISettings
from app.services.profile_service import ensure_default_profile


@asynccontextmanager
async def lifespan(app: FastAPI):
    run_migrations()
    settings = get_settings()
    Path(settings.storage_path).mkdir(parents=True, exist_ok=True)
    db = SessionLocal()
    try:
        if not db.get(AISettings, 1):
            db.add(AISettings())
            db.commit()
        ensure_default_profile(db)
    finally:
        db.close()
    yield


app = FastAPI(
    title="EPP Sentinel API",
    version="1.4.0",
    lifespan=lifespan,
)

settings = get_settings()
origins = [o.strip() for o in settings.cors_origins.split(",") if o.strip()]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

api = FastAPI()
prefix = "/api/v1"

app.include_router(auth.router, prefix=prefix)
app.include_router(devices.router, prefix=prefix)
app.include_router(captures.router, prefix=prefix)
app.include_router(profiles.router, prefix=prefix)
app.include_router(ai_settings.router, prefix=prefix)
app.include_router(analyses.router, prefix=prefix)
app.include_router(audit_logs.router, prefix=prefix)

storage_dir = Path(settings.storage_path)
storage_dir.mkdir(parents=True, exist_ok=True)
app.mount("/storage", StaticFiles(directory=str(storage_dir)), name="storage")


@app.get("/health")
def health():
    return {"status": "ok"}
