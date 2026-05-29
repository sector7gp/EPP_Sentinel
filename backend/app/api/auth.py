from fastapi import APIRouter, Depends, HTTPException, status

from app.config import get_settings
from app.schemas.schemas import LoginRequest, TokenResponse
from app.utils.auth import create_access_token, verify_password

router = APIRouter(prefix="/auth", tags=["auth"])


@router.post("/login", response_model=TokenResponse)
def login(body: LoginRequest):
    settings = get_settings()
    if body.username != settings.admin_username or body.password != settings.admin_password:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Credenciales inválidas")
    token = create_access_token(body.username)
    return TokenResponse(access_token=token)
