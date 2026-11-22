"""Inicialización y consultas de PostgreSQL para las lecturas del monitor."""

from collections.abc import Sequence
from pathlib import Path

import psycopg
from psycopg.rows import dict_row
from psycopg_pool import ConnectionPool

from .config import LOCAL_TIME_ZONE, database_url

SCHEMA = Path(__file__).with_name("schema.sql")
READING_FIELDS = "id, measured_at, received_at, temperature, humidity, gas, calibrated"

# El filtro de horas usa minutos completos en la zona del panel.
HISTORY_MINUTE = "date_trunc('minute', measured_at AT TIME ZONE %(zone)s::text)::time"
HISTORY_FILTER = f"""
    FROM readings
    WHERE device_id = %(device)s
      AND measured_at IS NOT NULL
      AND (%(start)s::timestamptz IS NULL OR measured_at >= %(start)s)
      AND (%(end)s::timestamptz IS NULL OR measured_at < %(end)s)
      AND CASE
          WHEN %(start_time)s::time > %(end_time)s::time
              THEN {HISTORY_MINUTE} >= %(start_time)s OR {HISTORY_MINUTE} <= %(end_time)s
          ELSE (%(start_time)s::time IS NULL OR {HISTORY_MINUTE} >= %(start_time)s)
              AND (%(end_time)s::time IS NULL OR {HISTORY_MINUTE} <= %(end_time)s)
      END
"""
HISTORY_ORDER = " ORDER BY measured_at DESC, received_at DESC, id DESC"


def init_db(conn) -> None:
    """Serializa la preparación del esquema entre los procesos que arrancan a la vez."""
    conn.execute(
        "SELECT pg_advisory_xact_lock(hashtext('monitor_aire'), hashtext(current_schema()))"
    )
    conn.execute(SCHEMA.read_text(encoding="utf-8"))


def open_pool() -> ConnectionPool:
    with psycopg.connect(database_url(), connect_timeout=5) as conn:
        init_db(conn)
    return ConnectionPool(
        database_url(),
        kwargs={"row_factory": dict_row, "connect_timeout": 5},
        min_size=1,
        max_size=8,
        timeout=5,
        open=False,
    )


INSERT_READING = """
    INSERT INTO readings
        (device_id, record_id, measured_at, temperature, humidity, gas, calibrated)
    VALUES (%(device_id)s, %(record_id)s, %(measured_at)s, %(temperature)s,
            %(humidity)s, %(gas)s, %(calibrated)s)
    ON CONFLICT (device_id, record_id) DO NOTHING
"""


def insert_readings(pool: ConnectionPool, device_id: str, readings: Sequence[dict]) -> int:
    """Registra un lote completo y devuelve las filas nuevas tras confirmar la transacción."""
    if not readings:
        return 0

    with pool.connection() as conn:
        conn.execute(
            "INSERT INTO devices (id) VALUES (%s) ON CONFLICT (id) DO NOTHING",
            (device_id,),
        )
        with conn.cursor() as cursor:
            cursor.executemany(INSERT_READING, readings)
            inserted = cursor.rowcount

    return inserted


def insert_reading(pool: ConnectionPool, reading: dict) -> bool:
    """Registra una lectura individual desde el cliente MQTT."""
    return insert_readings(pool, reading["device_id"], [reading]) == 1


def latest_by_device(pool: ConnectionPool) -> dict:
    """Busca una lectura reciente por dispositivo para construir el estado en vivo."""
    with pool.connection() as conn:
        rows = conn.execute(
            f"""
            SELECT devices.id AS device_id, reading.*
            FROM devices
            LEFT JOIN LATERAL (
                SELECT {READING_FIELDS} FROM readings
                WHERE device_id = devices.id
                  AND (measured_at IS NOT NULL OR record_id IS NULL)
                ORDER BY event_at DESC, id DESC LIMIT 1
            ) AS reading ON true
            ORDER BY devices.id
        """
        ).fetchall()
    return {row["device_id"]: row for row in rows}


def get_history(pool: ConnectionPool, filters: dict, page: int, per_page: int) -> dict:
    params = {**filters, "limit": per_page, "offset": (page - 1) * per_page}
    with pool.connection() as conn:
        # El total y la página deben ver la misma versión de las lecturas.
        conn.execute("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ READ ONLY")
        total = conn.execute(
            "SELECT count(*) AS total " + HISTORY_FILTER,
            filters,
        ).fetchone()["total"]
        rows = conn.execute(
            f"SELECT device_id, {READING_FIELDS} "
            + HISTORY_FILTER
            + HISTORY_ORDER
            + " LIMIT %(limit)s OFFSET %(offset)s",
            params,
        ).fetchall()
    return {"rows": rows, "total": total, "page": page, "per_page": per_page}


def export_history(pool: ConnectionPool, filters: dict):
    """Envía el CSV por bloques en la zona horaria del panel."""
    # El apóstrofo protege los identificadores que una hoja de cálculo leería como fórmulas.
    query = (
        """
        SELECT CASE WHEN ltrim(device_id) ~ '^[=+@＝＋－＠-]'
                    THEN chr(39) || device_id ELSE device_id END AS device_id,
               measured_at, received_at, temperature AS temperature_c,
               humidity AS humidity_rh, gas AS gas_ppm, calibrated
        """
        + HISTORY_FILTER
        + HISTORY_ORDER
    )

    with pool.connection() as conn, conn.cursor() as cursor:
        cursor.execute("SELECT set_config('TimeZone', %s, true)", (LOCAL_TIME_ZONE,))
        with cursor.copy(
            f"COPY ({query}) TO STDOUT WITH (FORMAT CSV, HEADER TRUE)",
            filters,
        ) as copy:
            # El marcador UTF-8 facilita la lectura de acentos en hojas de cálculo.
            chunk = bytearray(b"\xef\xbb\xbf")
            for row in copy:
                chunk.extend(row)
                if len(chunk) >= 32768:
                    yield bytes(chunk)
                    chunk.clear()
            if chunk:
                yield bytes(chunk)


def get_series(pool: ConnectionPool, device: str, start, end, step) -> list:
    """Agrupa las mediciones en intervalos desde el inicio de la consulta."""
    with pool.connection() as conn:
        return conn.execute(
            """
            SELECT date_bin(%(step)s, measured_at, %(start)s) AS bucket,
                   avg(temperature) AS temperature, avg(humidity) AS humidity, avg(gas) AS gas
            FROM readings
            WHERE device_id = %(device)s AND measured_at >= %(start)s AND measured_at <= %(end)s
            GROUP BY bucket ORDER BY bucket
        """,
            {"device": device, "start": start, "end": end, "step": step},
        ).fetchall()
