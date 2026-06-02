"""Tests for stream validation and multi-stream config."""

import pytest
from fastapi import HTTPException

from app.services.stream_service import validate_stream_config


def test_usb_config_default_device():
    cfg = validate_stream_config("usb", {})
    assert cfg["device"] == "/dev/video0"


def test_usb_config_custom_device():
    cfg = validate_stream_config("usb", {"device": "/dev/video2"})
    assert cfg["device"] == "/dev/video2"


def test_rtsp_config_valid():
    cfg = validate_stream_config("rtsp", {"url": "rtsp://admin:pass@10.0.0.1:554/stream"})
    assert cfg["url"].startswith("rtsp://")


def test_rtsp_config_invalid():
    with pytest.raises(HTTPException):
        validate_stream_config("rtsp", {"url": "http://invalid"})


def test_invalid_source_type():
    with pytest.raises(HTTPException):
        validate_stream_config("hik_connect", {})
