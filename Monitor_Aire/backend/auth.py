"""Validación de credenciales, sesiones con JWT y cookies de la API."""

import os
import re
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone

import jwt
from fastapi import APIRouter, HTTPException, Request, Response
from pwdlib import PasswordHash
from pydantic import BaseModel, Field

from .config import env_flag

COOKIE = "monitor_aire_session"
ISSUER = "monitor-aire"
PASSWORD_HASH = PasswordHash.recommended()
# Las cuentas inexistentes también pasan por una verificación de contraseña.
DUMMY_HASH = PASSWORD_HASH.hash("monitor-aire: cuenta de comprobación")
USERNAME = re.compile(r"[a-z0-9][a-z0-9_.-]{2,63}")
router = APIRouter(prefix="/auth")


@dataclass(frozen=True)
class AuthSettings:
    secret: str
    minutes: int
    secure_cookie: bool

    @classmethod
    def load(cls):
        secret = os.getenv("JWT_SECRET", "")
        minutes = int(os.getenv("SESSION_MINUTES", "480"))
        if len(secret.encode()) < 32:
            raise RuntimeError("Configura JWT_SECRET con al menos 32 bytes en backend/.env")
        if minutes <= 0:
            raise RuntimeError("SESSION_MINUTES debe ser un entero positivo")
        return cls(secret, minutes, env_flag("COOKIE_SECURE"))


@dataclass(frozen=True)
class Session:
    username: str
    expires: datetime


class Credentials(BaseModel):
    username: str = Field(min_length=1, max_length=64)
    password: str = Field(min_length=1, max_length=256)


def session_required(request: Request) -> Session:
    """Verifica el token y confirma que su usuario siga registrado."""
    settings = request.app.state.auth
    try:
        claims = jwt.decode(
            request.cookies.get(COOKIE, ""),
            settings.secret,
            algorithms=["HS256"],
            issuer=ISSUER,
            options={"require": ["sub", "iat", "exp", "iss"]},
        )
    except jwt.InvalidTokenError:
        raise HTTPException(status_code=401, detail="Inicia sesión para continuar") from None

    with request.app.state.pool.connection() as conn:
        user = conn.execute(
            "SELECT username FROM users WHERE id::text = %s",
            (claims["sub"],),
        ).fetchone()
    if user is None:
        raise HTTPException(status_code=401, detail="Inicia sesión para continuar")
    return Session(user["username"], datetime.fromtimestamp(claims["exp"], timezone.utc))


@router.post("/login")
def login(credentials: Credentials, request: Request, response: Response):
    with request.app.state.pool.connection() as conn:
        user = conn.execute(
            "SELECT id, username, password_hash FROM users WHERE username = %s",
            (credentials.username.strip().lower(),),
        ).fetchone()
    valid = PASSWORD_HASH.verify(
        credentials.password,
        user["password_hash"] if user else DUMMY_HASH,
    )
    if not user or not valid:
        raise HTTPException(status_code=401, detail="Usuario o contraseña incorrectos")

    settings = request.app.state.auth
    now = datetime.now(timezone.utc)
    duration = timedelta(minutes=settings.minutes)
    token = jwt.encode(
        {"sub": str(user["id"]), "iat": now, "exp": now + duration, "iss": ISSUER},
        settings.secret,
        algorithm="HS256",
    )

    # La cookie HTTPOnly se limita a las rutas de la API.
    response.set_cookie(
        COOKIE,
        token,
        max_age=int(duration.total_seconds()),
        path="/api",
        httponly=True,
        secure=settings.secure_cookie,
        samesite="strict",
    )
    return {"username": user["username"]}


@router.post("/logout", status_code=204)
def logout(request: Request, response: Response):
    response.delete_cookie(
        COOKIE,
        path="/api",
        httponly=True,
        secure=request.app.state.auth.secure_cookie,
        samesite="strict",
    )


@router.get("/me")
def current_user(request: Request):
    return {"username": session_required(request).username}
