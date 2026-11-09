"""Convierte lecturas de dispositivo/+/crudo a dispositivo/<id>/tratado."""

from __future__ import annotations

import argparse
import csv
import io
import json
import logging
import math
import queue
import threading
from dataclasses import dataclass
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    import paho.mqtt.client as mqtt

RAW_TOPIC_FILTER = "dispositivo/+/crudo"
MESSAGE_QUEUE_SIZE = 100
PUBLISH_TIMEOUT_SECONDS = 10
MQTT_KEEPALIVE_SECONDS = 60
CSV_COLUMNS = (
    "id", "device_id", "timestamp", "temperature", "humidity", "gas", "calibrate"
)
MEASUREMENT_FIELDS = (
    ("temperature", "DHT11", "°C"),
    ("humidity", "DHT11", "%RH"),
    ("gas", "MQ135", "ppm"),
)
LOGGER = logging.getLogger(__name__)


@dataclass(frozen=True)
class ConvertedReading:
    """Lectura convertida y datos necesarios para confirmar su procesamiento."""

    device_id: str
    record_id: str
    json_payload: str


def parse_measurement(value: str) -> float | None:
    """Representa campos vacios como null y rechaza numeros no finitos."""
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


class CsvMqttBridge:
    """Procesa mensajes fuera del hilo MQTT para poder esperar el PUBACK."""

    def __init__(self, client: mqtt.Client) -> None:
        self.client = client
        self.messages: queue.Queue[tuple[str, bytes]] = queue.Queue(
            maxsize=MESSAGE_QUEUE_SIZE
        )
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message

    def on_connect(self, client, userdata, flags, reason_code, properties) -> None:
        """Restablece la suscripcion al conectar o reconectar al broker."""
        if reason_code != 0:
            LOGGER.error("Conexion rechazada: %s", reason_code)
            return

        client.subscribe(RAW_TOPIC_FILTER, qos=1)
        LOGGER.info("Conectado; escuchando %s", RAW_TOPIC_FILTER)

    def on_message(self, client, userdata, message) -> None:
        """Encola el CSV sin bloquear el procesamiento de paquetes MQTT."""
        try:
            self.messages.put_nowait((message.topic, message.payload))
        except queue.Full:
            LOGGER.warning("Cola llena; el ESP32 reintentara sin recibir ACK")

    def publish_reading(self, reading: ConvertedReading) -> None:
        """Confirma al ESP32 solo despues del PUBACK del JSON tratado."""
        topic_prefix = f"dispositivo/{reading.device_id}"
        publication = self.client.publish(
            f"{topic_prefix}/tratado", reading.json_payload, qos=1
        )
        publication.wait_for_publish(timeout=PUBLISH_TIMEOUT_SECONDS)

        if not publication.is_published():
            LOGGER.warning("Sin confirmacion del broker; CSV pendiente")
            return

        self.client.publish(f"{topic_prefix}/ack", reading.record_id, qos=1)

    def process_messages(self) -> None:
        """Mantiene pendientes en el ESP32 las lecturas que no se procesen."""
        while True:
            topic, payload = self.messages.get()
            try:
                reading = convert_csv_message(topic, payload)
                self.publish_reading(reading)
            except (ValueError, KeyError, UnicodeError, RuntimeError, csv.Error) as error:
                LOGGER.error("CSV no procesado: %s", error)
            finally:
                self.messages.task_done()

    def start(self) -> None:
        worker = threading.Thread(
            target=self.process_messages,
            name="csv-message-worker",
            daemon=True,
        )
        worker.start()


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--port", type=int, default=1883)
    return parser.parse_args()


def main() -> None:
    import paho.mqtt.client as mqtt

    arguments = parse_arguments()
    logging.basicConfig(level=logging.INFO)
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

    bridge = CsvMqttBridge(client)
    bridge.start()
    client.connect(arguments.host, arguments.port, MQTT_KEEPALIVE_SECONDS)

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        client.disconnect()


if __name__ == "__main__":
    main()
