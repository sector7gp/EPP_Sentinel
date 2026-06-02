import uuid
from datetime import datetime, time
from typing import List, Optional

from sqlalchemy import (
    Boolean,
    DateTime,
    ForeignKey,
    Integer,
    String,
    Text,
    Time,
    UniqueConstraint,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.database import Base


def _uuid() -> str:
    return str(uuid.uuid4())


class Device(Base):
    __tablename__ = "devices"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    name: Mapped[str] = mapped_column(String(128), nullable=False)
    location: Mapped[Optional[str]] = mapped_column(String(256), nullable=True)
    api_token: Mapped[str] = mapped_column(String(64), unique=True, nullable=False, index=True)
    last_seen_at: Mapped[Optional[datetime]] = mapped_column(DateTime, nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow)

    schedule: Mapped[Optional["CaptureSchedule"]] = relationship(back_populates="device", uselist=False)
    image_settings: Mapped[Optional["ImageSettings"]] = relationship(back_populates="device", uselist=False)
    profile_assignment: Mapped[Optional["DeviceProfileAssignment"]] = relationship(
        back_populates="device", uselist=False
    )
    streams: Mapped[List["VideoStream"]] = relationship(
        back_populates="device", cascade="all, delete-orphan", order_by="VideoStream.created_at"
    )
    captures: Mapped[List["Capture"]] = relationship(back_populates="device")


class VideoStream(Base):
    __tablename__ = "video_streams"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    device_id: Mapped[str] = mapped_column(String(36), ForeignKey("devices.id"), index=True)
    name: Mapped[str] = mapped_column(String(128), nullable=False)
    enabled: Mapped[bool] = mapped_column(Boolean, default=True)
    source_type: Mapped[str] = mapped_column(String(32), nullable=False)
    connection_config: Mapped[str] = mapped_column(Text, default="{}")
    profile_id: Mapped[str] = mapped_column(String(36), ForeignKey("operator_profiles.id"))
    created_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow)

    device: Mapped["Device"] = relationship(back_populates="streams")
    profile: Mapped["OperatorProfile"] = relationship(back_populates="streams")
    captures: Mapped[List["Capture"]] = relationship(back_populates="stream")


class CaptureSchedule(Base):
    __tablename__ = "capture_schedules"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    device_id: Mapped[str] = mapped_column(String(36), ForeignKey("devices.id"), unique=True)
    start_time: Mapped[time] = mapped_column(Time, default=time(7, 0))
    end_time: Mapped[time] = mapped_column(Time, default=time(18, 0))
    interval_value: Mapped[int] = mapped_column(Integer, default=5)
    interval_unit: Mapped[str] = mapped_column(String(10), default="minutes")
    enabled_days: Mapped[str] = mapped_column(String(32), default="0,1,2,3,4,5,6")

    device: Mapped["Device"] = relationship(back_populates="schedule")


class ImageSettings(Base):
    __tablename__ = "image_settings"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    device_id: Mapped[str] = mapped_column(String(36), ForeignKey("devices.id"), unique=True)
    width: Mapped[int] = mapped_column(Integer, default=1280)
    height: Mapped[int] = mapped_column(Integer, default=720)
    jpeg_quality: Mapped[int] = mapped_column(Integer, default=75)
    max_kb: Mapped[int] = mapped_column(Integer, default=500)

    device: Mapped["Device"] = relationship(back_populates="image_settings")


class OperatorProfile(Base):
    __tablename__ = "operator_profiles"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    name: Mapped[str] = mapped_column(String(128), nullable=False)
    description: Mapped[Optional[str]] = mapped_column(Text, nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow)

    epp_requirements: Mapped[List["ProfileEPPRequirement"]] = relationship(
        back_populates="profile", cascade="all, delete-orphan"
    )
    assignments: Mapped[List["DeviceProfileAssignment"]] = relationship(back_populates="profile")
    streams: Mapped[List["VideoStream"]] = relationship(back_populates="profile")


class ProfileEPPRequirement(Base):
    __tablename__ = "profile_epp_requirements"
    __table_args__ = (UniqueConstraint("profile_id", "epp_type", name="uq_profile_epp"),)

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    profile_id: Mapped[str] = mapped_column(String(36), ForeignKey("operator_profiles.id"))
    epp_type: Mapped[str] = mapped_column(String(64), nullable=False)

    profile: Mapped["OperatorProfile"] = relationship(back_populates="epp_requirements")


class DeviceProfileAssignment(Base):
    __tablename__ = "device_profile_assignments"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    device_id: Mapped[str] = mapped_column(String(36), ForeignKey("devices.id"), unique=True)
    profile_id: Mapped[str] = mapped_column(String(36), ForeignKey("operator_profiles.id"))

    device: Mapped["Device"] = relationship(back_populates="profile_assignment")
    profile: Mapped["OperatorProfile"] = relationship(back_populates="assignments")


class AISettings(Base):
    __tablename__ = "ai_settings"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, default=1)
    active_provider: Mapped[str] = mapped_column(String(32), default="openai")
    model_name: Mapped[str] = mapped_column(String(128), default="gpt-4o-mini")
    openai_api_key_enc: Mapped[Optional[str]] = mapped_column(Text, nullable=True)
    anthropic_api_key_enc: Mapped[Optional[str]] = mapped_column(Text, nullable=True)
    gemini_api_key_enc: Mapped[Optional[str]] = mapped_column(Text, nullable=True)
    updated_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)


class Capture(Base):
    __tablename__ = "captures"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    device_id: Mapped[str] = mapped_column(String(36), ForeignKey("devices.id"), index=True)
    stream_id: Mapped[Optional[str]] = mapped_column(String(36), ForeignKey("video_streams.id"), index=True)
    image_path: Mapped[str] = mapped_column(String(512), nullable=False)
    captured_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow, index=True)
    upload_status: Mapped[str] = mapped_column(String(32), default="received")
    file_size_kb: Mapped[Optional[int]] = mapped_column(Integer, nullable=True)

    device: Mapped["Device"] = relationship(back_populates="captures")
    stream: Mapped[Optional["VideoStream"]] = relationship(back_populates="captures")
    analysis: Mapped[Optional["Analysis"]] = relationship(back_populates="capture", uselist=False)


class Analysis(Base):
    __tablename__ = "analyses"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    capture_id: Mapped[str] = mapped_column(String(36), ForeignKey("captures.id"), unique=True)
    profile_id: Mapped[Optional[str]] = mapped_column(String(36), ForeignKey("operator_profiles.id"))
    provider: Mapped[str] = mapped_column(String(32))
    result_json: Mapped[str] = mapped_column(Text)
    cumple_normativa: Mapped[bool] = mapped_column(Boolean, default=False)
    observaciones: Mapped[Optional[str]] = mapped_column(Text, nullable=True)
    status: Mapped[str] = mapped_column(String(32), default="completed")
    analyzed_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow, index=True)

    capture: Mapped["Capture"] = relationship(back_populates="analysis")


class AuditLog(Base):
    __tablename__ = "audit_logs"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    event_type: Mapped[str] = mapped_column(String(64), index=True)
    message: Mapped[str] = mapped_column(Text)
    device_id: Mapped[Optional[str]] = mapped_column(String(36), ForeignKey("devices.id"), nullable=True)
    user: Mapped[Optional[str]] = mapped_column(String(128), nullable=True)
    payload_json: Mapped[Optional[str]] = mapped_column(Text, nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow, index=True)
