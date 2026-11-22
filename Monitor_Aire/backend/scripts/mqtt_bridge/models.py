"""Datos compartidos entre conversión y transporte."""

from dataclasses import dataclass


@dataclass(frozen=True)
class ConvertedReading:
    """Asocia el JSON tratado con el identificador original para enviar el ACK."""

    device_id: str
    record_id: str
    json_payload: str
