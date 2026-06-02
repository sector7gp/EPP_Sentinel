"""video streams and device location

Revision ID: 002
Revises: 001
Create Date: 2026-06-01

"""

import json
import uuid
from datetime import datetime
from typing import Sequence, Union

import sqlalchemy as sa
from alembic import op

revision: str = "002"
down_revision: Union[str, None] = "001"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def _has_column(conn, table: str, column: str) -> bool:
    rows = conn.execute(sa.text(f"PRAGMA table_info({table})")).fetchall()
    return any(row[1] == column for row in rows)


def _has_table(conn, table: str) -> bool:
    row = conn.execute(
        sa.text("SELECT name FROM sqlite_master WHERE type='table' AND name=:name"),
        {"name": table},
    ).fetchone()
    return row is not None


def _has_index(conn, index: str) -> bool:
    row = conn.execute(
        sa.text("SELECT name FROM sqlite_master WHERE type='index' AND name=:name"),
        {"name": index},
    ).fetchone()
    return row is not None


def upgrade() -> None:
    conn = op.get_bind()

    if not _has_column(conn, "devices", "location"):
        op.add_column("devices", sa.Column("location", sa.String(256), nullable=True))

    if not _has_table(conn, "video_streams"):
        op.create_table(
            "video_streams",
            sa.Column("id", sa.String(36), primary_key=True),
            sa.Column("device_id", sa.String(36), sa.ForeignKey("devices.id"), nullable=False),
            sa.Column("name", sa.String(128), nullable=False),
            sa.Column("enabled", sa.Boolean(), nullable=False, server_default=sa.true()),
            sa.Column("source_type", sa.String(32), nullable=False),
            sa.Column("connection_config", sa.Text(), nullable=False, server_default="{}"),
            sa.Column("profile_id", sa.String(36), sa.ForeignKey("operator_profiles.id"), nullable=False),
            sa.Column("created_at", sa.DateTime(), nullable=False),
        )

    if not _has_index(conn, "ix_video_streams_device_id"):
        op.create_index("ix_video_streams_device_id", "video_streams", ["device_id"])

    if not _has_column(conn, "captures", "stream_id"):
        # SQLite no soporta ADD COLUMN con FK; la columna es nullable y se valida en la app.
        op.add_column(
            "captures",
            sa.Column("stream_id", sa.String(36), nullable=True),
        )

    if not _has_index(conn, "ix_captures_stream_id"):
        op.create_index("ix_captures_stream_id", "captures", ["stream_id"])

    default_profile = conn.execute(
        sa.text("SELECT id FROM operator_profiles ORDER BY created_at ASC LIMIT 1")
    ).fetchone()
    if not default_profile:
        return

    default_profile_id = default_profile[0]
    devices = conn.execute(sa.text("SELECT id FROM devices")).fetchall()
    for (device_id,) in devices:
        existing = conn.execute(
            sa.text("SELECT COUNT(*) FROM video_streams WHERE device_id = :device_id"),
            {"device_id": device_id},
        ).scalar()
        if existing and existing > 0:
            continue

        assignment = conn.execute(
            sa.text(
                "SELECT profile_id FROM device_profile_assignments WHERE device_id = :device_id"
            ),
            {"device_id": device_id},
        ).fetchone()
        profile_id = assignment[0] if assignment else default_profile_id
        stream_id = str(uuid.uuid4())
        conn.execute(
            sa.text(
                """
                INSERT INTO video_streams
                (id, device_id, name, enabled, source_type, connection_config, profile_id, created_at)
                VALUES (:id, :device_id, :name, 1, 'usb', :config, :profile_id, :created_at)
                """
            ),
            {
                "id": stream_id,
                "device_id": device_id,
                "name": "Cámara principal",
                "config": json.dumps({"device": "/dev/video0"}),
                "profile_id": profile_id,
                "created_at": datetime.utcnow(),
            },
        )


def downgrade() -> None:
    conn = op.get_bind()
    if _has_index(conn, "ix_captures_stream_id"):
        op.drop_index("ix_captures_stream_id", table_name="captures")
    if _has_column(conn, "captures", "stream_id"):
        op.drop_column("captures", "stream_id")
    if _has_index(conn, "ix_video_streams_device_id"):
        op.drop_index("ix_video_streams_device_id", table_name="video_streams")
    if _has_table(conn, "video_streams"):
        op.drop_table("video_streams")
    if _has_column(conn, "devices", "location"):
        op.drop_column("devices", "location")
