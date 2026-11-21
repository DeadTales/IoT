"""Datos compartidos entre conversion y transporte."""

from dataclasses import dataclass


@dataclass(frozen=True)
class ConvertedReading:
    """Lectura convertida y datos necesarios para confirmar su procesamiento."""

    device_id: str
    record_id: str
    json_payload: str
