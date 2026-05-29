"""initial schema

Revision ID: 001
Revises:
Create Date: 2026-05-28

"""

from typing import Sequence, Union

import sqlalchemy as sa
from alembic import op

revision: str = "001"
down_revision: Union[str, None] = None
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        "devices",
        sa.Column("id", sa.String(36), primary_key=True),
        sa.Column("name", sa.String(128), nullable=False),
        sa.Column("api_token", sa.String(64), nullable=False),
        sa.Column("last_seen_at", sa.DateTime(), nullable=True),
        sa.Column("created_at", sa.DateTime(), nullable=False),
    )
    op.create_index("ix_devices_api_token", "devices", ["api_token"], unique=True)

    op.create_table(
        "operator_profiles",
        sa.Column("id", sa.String(36), primary_key=True),
        sa.Column("name", sa.String(128), nullable=False),
        sa.Column("description", sa.Text(), nullable=True),
        sa.Column("created_at", sa.DateTime(), nullable=False),
    )

    op.create_table(
        "ai_settings",
        sa.Column("id", sa.Integer(), primary_key=True),
        sa.Column("active_provider", sa.String(32), nullable=False),
        sa.Column("model_name", sa.String(128), nullable=False),
        sa.Column("openai_api_key_enc", sa.Text(), nullable=True),
        sa.Column("anthropic_api_key_enc", sa.Text(), nullable=True),
        sa.Column("gemini_api_key_enc", sa.Text(), nullable=True),
        sa.Column("updated_at", sa.DateTime(), nullable=False),
    )

    op.create_table(
        "capture_schedules",
        sa.Column("id", sa.String(36), primary_key=True),
        sa.Column("device_id", sa.String(36), sa.ForeignKey("devices.id"), unique=True),
        sa.Column("start_time", sa.Time(), nullable=False),
        sa.Column("end_time", sa.Time(), nullable=False),
        sa.Column("interval_value", sa.Integer(), nullable=False),
        sa.Column("interval_unit", sa.String(10), nullable=False),
        sa.Column("enabled_days", sa.String(32), nullable=False),
    )

    op.create_table(
        "image_settings",
        sa.Column("id", sa.String(36), primary_key=True),
        sa.Column("device_id", sa.String(36), sa.ForeignKey("devices.id"), unique=True),
        sa.Column("width", sa.Integer(), nullable=False),
        sa.Column("height", sa.Integer(), nullable=False),
        sa.Column("jpeg_quality", sa.Integer(), nullable=False),
        sa.Column("max_kb", sa.Integer(), nullable=False),
    )

    op.create_table(
        "profile_epp_requirements",
        sa.Column("id", sa.String(36), primary_key=True),
        sa.Column("profile_id", sa.String(36), sa.ForeignKey("operator_profiles.id")),
        sa.Column("epp_type", sa.String(64), nullable=False),
        sa.UniqueConstraint("profile_id", "epp_type", name="uq_profile_epp"),
    )

    op.create_table(
        "device_profile_assignments",
        sa.Column("id", sa.String(36), primary_key=True),
        sa.Column("device_id", sa.String(36), sa.ForeignKey("devices.id"), unique=True),
        sa.Column("profile_id", sa.String(36), sa.ForeignKey("operator_profiles.id")),
    )

    op.create_table(
        "captures",
        sa.Column("id", sa.String(36), primary_key=True),
        sa.Column("device_id", sa.String(36), sa.ForeignKey("devices.id")),
        sa.Column("image_path", sa.String(512), nullable=False),
        sa.Column("captured_at", sa.DateTime(), nullable=False),
        sa.Column("upload_status", sa.String(32), nullable=False),
        sa.Column("file_size_kb", sa.Integer(), nullable=True),
    )
    op.create_index("ix_captures_device_id", "captures", ["device_id"])
    op.create_index("ix_captures_captured_at", "captures", ["captured_at"])

    op.create_table(
        "analyses",
        sa.Column("id", sa.String(36), primary_key=True),
        sa.Column("capture_id", sa.String(36), sa.ForeignKey("captures.id"), unique=True),
        sa.Column("profile_id", sa.String(36), sa.ForeignKey("operator_profiles.id"), nullable=True),
        sa.Column("provider", sa.String(32), nullable=False),
        sa.Column("result_json", sa.Text(), nullable=False),
        sa.Column("cumple_normativa", sa.Boolean(), nullable=False),
        sa.Column("observaciones", sa.Text(), nullable=True),
        sa.Column("status", sa.String(32), nullable=False),
        sa.Column("analyzed_at", sa.DateTime(), nullable=False),
    )
    op.create_index("ix_analyses_analyzed_at", "analyses", ["analyzed_at"])

    op.create_table(
        "audit_logs",
        sa.Column("id", sa.String(36), primary_key=True),
        sa.Column("event_type", sa.String(64), nullable=False),
        sa.Column("message", sa.Text(), nullable=False),
        sa.Column("device_id", sa.String(36), sa.ForeignKey("devices.id"), nullable=True),
        sa.Column("user", sa.String(128), nullable=True),
        sa.Column("payload_json", sa.Text(), nullable=True),
        sa.Column("created_at", sa.DateTime(), nullable=False),
    )
    op.create_index("ix_audit_logs_event_type", "audit_logs", ["event_type"])
    op.create_index("ix_audit_logs_created_at", "audit_logs", ["created_at"])


def downgrade() -> None:
    op.drop_table("audit_logs")
    op.drop_table("analyses")
    op.drop_table("captures")
    op.drop_table("device_profile_assignments")
    op.drop_table("profile_epp_requirements")
    op.drop_table("image_settings")
    op.drop_table("capture_schedules")
    op.drop_table("ai_settings")
    op.drop_table("operator_profiles")
    op.drop_table("devices")
