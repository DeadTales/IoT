"""Crea una cuenta local del dashboard."""

import argparse
from getpass import getpass

import psycopg

from .auth import PASSWORD_HASH, USERNAME
from .config import database_url
from .db import init_db


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("username", help="Nombre de usuario de 3 a 64 caracteres")
    args = parser.parse_args()
    username = args.username.lower()
    if not USERNAME.fullmatch(username):
        parser.error("Usa de 3 a 64 caracteres: letras a-z, números, puntos, guiones o guiones bajos")

    password = getpass("Contraseña: ")
    if not 12 <= len(password) <= 256:
        parser.error("La contraseña debe tener entre 12 y 256 caracteres")
    if password != getpass("Confirma la contraseña: "):
        parser.error("Las contraseñas no coinciden")

    password_hash = PASSWORD_HASH.hash(password)
    with psycopg.connect(database_url(), connect_timeout=5) as conn:
        init_db(conn)
        created = conn.execute(
            """INSERT INTO users (username, password_hash) VALUES (%s, %s)
               ON CONFLICT (username) DO NOTHING""", (username, password_hash),
        ).rowcount
    if not created:
        parser.error("Ese usuario ya existe")
    print(f"Usuario {username} creado")


if __name__ == "__main__":
    main()
