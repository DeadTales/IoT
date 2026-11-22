"""Validación y conversión CSV a JSON en memoria."""

from __future__ import annotations

import csv
import io
import json
import math

from .models import ConvertedReading
from .settings import CSV_COLUMNS, MEASUREMENT_FIELDS


def parse_measurement(value: str) -> float | None:
    """Representa campos vacíos como null y rechaza números no finitos."""
    if not value:
        return None

    number = float(value)
    if not math.isfinite(number):
        raise ValueError("Numero no finito")

    return number


def convert_csv_message(topic: str, payload: bytes) -> ConvertedReading:
    """Valida una fila del CSV y genera el mismo esquema JSON del firmware."""
    topic_parts = topic.split("/")
    if (
        len(topic_parts) != 3
        or topic_parts[0] != "dispositivo"
        or not topic_parts[1]
        or topic_parts[2] != "crudo"
    ):
        raise ValueError("Topic no valido")

    # El firmware envía una cabecera y una fila completa en cada mensaje.
    reader = csv.DictReader(io.StringIO(payload.decode("utf-8")), strict=True)
    if reader.fieldnames != list(CSV_COLUMNS):
        raise ValueError("Cabecera CSV no valida")

    rows = list(reader)
    if len(rows) != 1:
        raise ValueError("Se espera una lectura por mensaje CSV")

    row = rows[0]
    if None in row or any(value is None for value in row.values()):
        raise ValueError("Cantidad de columnas CSV no valida")

    device_id = topic_parts[1]
    if row["device_id"] != device_id or not row["id"]:
        raise ValueError("ID no coincide con el topic o falta el ID del registro")
    if row["calibrate"] not in ("true", "false"):
        raise ValueError("calibrate invalido")

    # La tabla de metadatos mantiene los sensores y las unidades del contrato JSON.
    measurements = {
        name: {
            "sensor": sensor,
            "value": parse_measurement(row[name]),
            "unit": unit,
        }
        for name, sensor, unit in MEASUREMENT_FIELDS
    }
    measurements["gas"]["calibrate"] = row["calibrate"] == "true"
    document = {
        "device_id": device_id,
        "timestamp": row["timestamp"] or None,
        "measurements": measurements,
    }

    return ConvertedReading(
        device_id=device_id,
        record_id=row["id"],
        json_payload=json.dumps(document, ensure_ascii=False, allow_nan=False),
    )
