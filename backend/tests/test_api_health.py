from fastapi.testclient import TestClient

from app.main import app

client = TestClient(app)


def test_health():
    response = client.get("/health")
    assert response.status_code == 200
    assert response.json()["status"] == "ok"


def test_login_invalid():
    response = client.post(
        "/api/v1/auth/login",
        json={"username": "wrong", "password": "wrong"},
    )
    assert response.status_code == 401


def test_login_success():
    response = client.post(
        "/api/v1/auth/login",
        json={"username": "admin", "password": "admin"},
    )
    assert response.status_code == 200
    assert "access_token" in response.json()
