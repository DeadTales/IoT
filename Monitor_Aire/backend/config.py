"""Carga de la configuración local del backend."""

import os
from pathlib import Path

from dotenv import load_dotenv

# Permite iniciar la aplicación desde cualquier directorio.
load_dotenv(Path(__file__).with_name(".env"))


def database_url() -> str:
    url = os.getenv("DATABASE_URL")
    if not url:
        raise RuntimeError("Configura DATABASE_URL en backend/.env")
    return url


def env_flag(name: str, default: bool = False) -> bool:
    return os.getenv(name, str(default)).lower() == "true"
