from datetime import datetime, time
from typing import Dict, List, Optional

from pydantic import BaseModel, Field

from app.constants import ALL_EPP_TYPES


class TokenResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"


class LoginRequest(BaseModel):
    username: str
    password: str


class DeviceCreate(BaseModel):
    name: str = Field(min_length=1, max_length=128)
    location: Optional[str] = Field(default=None, max_length=256)


class DeviceUpdate(BaseModel):
    name: Optional[str] = Field(default=None, min_length=1, max_length=128)
    location: Optional[str] = Field(default=None, max_length=256)


class DeviceResponse(BaseModel):
    id: str
    name: str
    location: Optional[str] = None
    api_token: str
    last_seen_at: Optional[datetime] = None
    online: bool = False
    stream_count: int = 0

    model_config = {"from_attributes": True}


class ScheduleUpdate(BaseModel):
    start_time: time = time(7, 0)
    end_time: time = time(18, 0)
    interval_value: int = Field(default=5, ge=1)
    interval_unit: str = Field(default="minutes", pattern="^(seconds|minutes)$")
    enabled_days: str = "0,1,2,3,4,5,6"


class ScheduleResponse(ScheduleUpdate):
    device_id: str

    model_config = {"from_attributes": True}


class ImageSettingsUpdate(BaseModel):
    width: int = Field(default=1280, ge=320, le=2560)
    height: int = Field(default=720, ge=240, le=1920)
    jpeg_quality: int = Field(default=75, ge=30, le=100)
    max_kb: int = Field(default=500, ge=50, le=5000)


class ImageSettingsResponse(ImageSettingsUpdate):
    device_id: str

    model_config = {"from_attributes": True}


class ProfileCreate(BaseModel):
    name: str
    description: Optional[str] = None
    required_epp: List[str] = Field(default_factory=list)


class ProfileUpdate(BaseModel):
    name: Optional[str] = None
    description: Optional[str] = None
    required_epp: Optional[List[str]] = None


class ProfileResponse(BaseModel):
    id: str
    name: str
    description: Optional[str]
    required_epp: List[str]

    model_config = {"from_attributes": True}


class ProfileAssignRequest(BaseModel):
    profile_id: str


class AISettingsUpdate(BaseModel):
    active_provider: Optional[str] = None
    model_name: Optional[str] = None
    openai_api_key: Optional[str] = None
    anthropic_api_key: Optional[str] = None
    gemini_api_key: Optional[str] = None


class AISettingsResponse(BaseModel):
    active_provider: str
    model_name: str
    openai_api_key_set: bool
    anthropic_api_key_set: bool
    gemini_api_key_set: bool
    openai_api_key_masked: Optional[str] = None
    anthropic_api_key_masked: Optional[str] = None
    gemini_api_key_masked: Optional[str] = None


class DeviceConfigResponse(BaseModel):
    device_id: str
    schedule: ScheduleResponse
    image_settings: ImageSettingsResponse
    streams: List["StreamConfigResponse"]
    config_version: str


class StreamConnectionConfig(BaseModel):
    device: Optional[str] = None
    url: Optional[str] = None


class StreamCreate(BaseModel):
    name: str = Field(min_length=1, max_length=128)
    source_type: str = Field(pattern="^(usb|rtsp)$")
    connection_config: StreamConnectionConfig
    profile_id: str
    enabled: bool = True


class StreamUpdate(BaseModel):
    name: Optional[str] = Field(default=None, min_length=1, max_length=128)
    source_type: Optional[str] = Field(default=None, pattern="^(usb|rtsp)$")
    connection_config: Optional[StreamConnectionConfig] = None
    profile_id: Optional[str] = None
    enabled: Optional[bool] = None


class StreamResponse(BaseModel):
    id: str
    device_id: str
    name: str
    enabled: bool
    source_type: str
    connection_config: StreamConnectionConfig
    profile_id: str
    profile_name: Optional[str] = None
    required_epp: List[str] = Field(default_factory=list)

    model_config = {"from_attributes": True}


class StreamConfigResponse(BaseModel):
    id: str
    name: str
    enabled: bool
    source_type: str
    connection_config: StreamConnectionConfig
    profile_id: str


class CaptureUploadResponse(BaseModel):
    capture_id: str
    status: str
    analysis_id: Optional[str] = None


class EPPAnalysisResult(BaseModel):
    casco_seguridad: Optional[bool] = None
    gafas_seguridad: Optional[bool] = None
    proteccion_auditiva: Optional[bool] = None
    guantes_seguridad: Optional[bool] = None
    calzado_seguridad: Optional[bool] = None
    ropa_industrial: Optional[bool] = None
    proteccion_respiratoria: Optional[bool] = None
    chaleco_reflectivo: Optional[bool] = None
    cumple_normativa: bool = False
    observaciones: str = ""

    def to_result_dict(self) -> dict:
        data = self.model_dump()
        return {k: v for k, v in data.items() if k in ALL_EPP_TYPES or k in ("cumple_normativa", "observaciones")}


class AnalysisResponse(BaseModel):
    id: str
    capture_id: str
    device_id: str
    device_name: str
    stream_id: Optional[str] = None
    stream_name: Optional[str] = None
    image_url: str
    captured_at: datetime
    analyzed_at: datetime
    provider: str
    cumple_normativa: bool
    observaciones: Optional[str]
    epp_results: Dict[str, bool]
    required_epp: List[str] = Field(default_factory=list)
    status: str

    model_config = {"from_attributes": True}


class StreamDashboardItem(BaseModel):
    stream_id: str
    stream_name: str
    device_id: str
    device_name: str
    device_location: Optional[str]
    device_online: bool
    profile_id: str
    profile_name: str
    required_epp: List[str]
    latest_analysis: Optional[AnalysisResponse] = None


class AnalysisListResponse(BaseModel):
    items: List[AnalysisResponse]
    total: int
    page: int
    page_size: int


class AuditLogResponse(BaseModel):
    id: str
    event_type: str
    message: str
    device_id: Optional[str]
    user: Optional[str]
    payload_json: Optional[str]
    created_at: datetime

    model_config = {"from_attributes": True}


class AuditLogListResponse(BaseModel):
    items: List[AuditLogResponse]
    total: int
