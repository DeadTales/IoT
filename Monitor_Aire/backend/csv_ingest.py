"""Recepción de archivos CSV por bloques con una clave compartida con el firmware."""

import csv
import os
from itertools import islice
from secrets import compare_digest

from fastapi import APIRouter, HTTPException, Request, Response
from fastapi.concurrency import run_in_threadpool

from .db import insert_readings
from .mqtt_ingest import parse_reading, valid_id
from .scripts.mqtt_bridge.converter import parse_csv_row, read_csv_rows

MAX_CSV_BYTES = 8192
MAX_CSV_RECORDS = 50

router = APIRouter(prefix="/ingest")


@router.post("/{device_id}/csv", status_code=204)
async def upload_csv(device_id: str, request: Request) -> Response:
    expected_token = os.getenv("CSV_UPLOAD_TOKEN", "")
    if not expected_token:
        raise HTTPException(status_code=503, detail="La subida CSV por HTTP está deshabilitada")

    supplied_token = request.headers.get("X-CSV-Token", "")
    if not compare_digest(expected_token.encode(), supplied_token.encode()):
        raise HTTPException(status_code=401, detail="Token CSV incorrecto")

    content_type = request.headers.get("Content-Type", "").partition(";")[0].strip().lower()
    if content_type != "text/csv":
        raise HTTPException(status_code=415, detail="Se requiere Content-Type: text/csv")
    if not valid_id(device_id):
        raise HTTPException(status_code=422, detail="Identificador de dispositivo inválido")

    payload = bytearray()
    async for chunk in request.stream():
        if len(payload) + len(chunk) > MAX_CSV_BYTES:
            raise HTTPException(status_code=413, detail="El CSV supera el límite de 8192 bytes")
        payload.extend(chunk)

    # Valida todo el bloque antes de abrir la transacción de escritura.
    try:
        rows = list(islice(read_csv_rows(bytes(payload)), MAX_CSV_RECORDS + 1))
        if len(rows) > MAX_CSV_RECORDS:
            raise HTTPException(status_code=413, detail="El CSV supera el límite de 50 registros")
        if not rows:
            raise ValueError("El CSV debe contener al menos un registro")
        readings = [parse_reading(device_id, parse_csv_row(device_id, row)) for row in rows]
    except (csv.Error, ValueError, OverflowError) as error:
        raise HTTPException(status_code=422, detail="El CSV contiene datos inválidos") from error

    inserted = await run_in_threadpool(insert_readings, request.app.state.pool, device_id, readings)
    if inserted > 0:
        request.app.state.live.notify()

    # El ESP32 confirma el bloque completo, incluidos los registros que ya existían.
    return Response(status_code=204, headers={"X-CSV-Records": str(len(readings))})
