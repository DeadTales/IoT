"""Recibe telemetría MQTT y guarda las lecturas del monitor."""

from __future__ import annotations

import csv
import json
import logging
import math
import os
import queue
import threading
from collections.abc import Callable
from datetime import datetime, timezone
from typing import TYPE_CHECKING

import paho.mqtt.client as mqtt
import psycopg

from .config import env_flag
from .db import insert_reading
from .scripts.mqtt_bridge.converter import convert_csv_message
from .scripts.mqtt_bridge.settings import (
    MEASUREMENT_FIELDS,
    MESSAGE_QUEUE_SIZE,
    MQTT_KEEPALIVE_SECONDS,
    PUBLISH_TIMEOUT_SECONDS,
    RAW_TOPIC_FILTER,
)

if TYPE_CHECKING:
    from psycopg_pool import ConnectionPool

LOGGER = logging.getLogger(__name__)
PROCESSED_TOPIC = "dispositivo/+/tratado"
STATUS_TOPIC = "dispositivo/+/estado"
MAX_PAYLOAD_BYTES = 4096


def valid_id(value: object) -> bool:
    return (
        isinstance(value, str)
        and 0 < len(value) <= 128
        and value.isprintable()
        and not any(character in value for character in "/+#")
    )


def parse_reading(device_id: str, document: dict) -> dict:
    if not isinstance(document, dict) or document.get("device_id") != device_id:
        raise ValueError("El dispositivo del JSON no coincide con el tema MQTT")

    timestamp = document["timestamp"]
    measured_at = None
    if timestamp is not None:
        if not isinstance(timestamp, str):
            raise ValueError("Fecha de medición inválida")
        measured_at = datetime.fromisoformat(timestamp)
        if measured_at.tzinfo is None:
            raise ValueError("Falta el desplazamiento de la zona horaria")
        measured_at = measured_at.astimezone(timezone.utc)

    record_id = document.get("record_id")
    if record_id is not None and not valid_id(record_id):
        raise ValueError("ID de registro inválido")

    measurements = document["measurements"]
    values = {}
    for name, _sensor, _unit in MEASUREMENT_FIELDS:
        value = measurements[name]["value"]
        if value is not None:
            if type(value) not in (int, float) or not math.isfinite(value):
                raise ValueError(f"Medición inválida: {name}")
            value = float(value)
        values[name] = value

    calibrated = measurements["gas"]["calibrate"]
    if not isinstance(calibrated, bool):
        raise ValueError("Indicador de calibración inválido")

    return {
        "device_id": device_id,
        "record_id": record_id,
        "measured_at": measured_at,
        "calibrated": calibrated,
        **values,
    }


class MqttIngest:
    def __init__(
        self,
        pool: ConnectionPool,
        on_broker: Callable[[bool], None],
        on_status: Callable[[str, str], None],
        on_reading: Callable[[], None],
    ) -> None:
        self.pool = pool
        self.on_broker = on_broker
        self.on_status = on_status
        self.on_reading = on_reading
        self.host = os.getenv("MQTT_HOST", "127.0.0.1")
        self.port = int(os.getenv("MQTT_PORT", "1883"))
        self.messages: queue.Queue[tuple[str, str, bytes]] = queue.Queue(MESSAGE_QUEUE_SIZE)
        self.stopping = threading.Event()
        self.worker = threading.Thread(target=self.process_messages, name="mqtt-ingest")
        self.topics = [(PROCESSED_TOPIC, 1), (STATUS_TOPIC, 1)]
        if env_flag("MQTT_CONVERT_RAW", True):
            self.topics.append((RAW_TOPIC_FILTER, 1))

        self.client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id=os.getenv("MQTT_CLIENT_ID", "monitor-aire-dashboard"),
        )
        self.client.on_connect = self.on_connect
        self.client.on_subscribe = self.on_subscribe
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message
        self.client.reconnect_delay_set(min_delay=1, max_delay=30)
        self.client.max_queued_messages_set(MESSAGE_QUEUE_SIZE)

    def start(self) -> None:
        self.client.connect_async(self.host, self.port, MQTT_KEEPALIVE_SECONDS)
        self.worker.start()
        self.client.loop_start()

    def stop(self) -> None:
        self.stopping.set()
        self.client.disconnect()
        self.client.loop_stop()
        self.worker.join()

    def on_connect(self, client, userdata, flags, reason_code, properties) -> None:
        if reason_code.is_failure:
            LOGGER.warning("El broker MQTT rechazó la conexión: %s", reason_code)
            return
        result, _mid = client.subscribe(self.topics)
        if result != mqtt.MQTT_ERR_SUCCESS:
            LOGGER.warning("No se pudo solicitar la suscripción MQTT: %s", result)

    def on_subscribe(self, client, userdata, mid, reason_codes, properties) -> None:
        connected = not any(code.is_failure for code in reason_codes)
        if not connected:
            LOGGER.warning("El broker rechazó al menos una suscripción MQTT: %s", reason_codes)
        self.on_broker(connected)

    def on_disconnect(self, client, userdata, flags, reason_code, properties) -> None:
        self.on_broker(False)

    def on_message(self, client, userdata, message) -> None:
        parts = message.topic.split("/")
        if len(parts) != 3 or parts[0] != "dispositivo" or not valid_id(parts[1]):
            LOGGER.warning("Se descartó un tema MQTT inválido")
            return
        _, device_id, kind = parts
        if kind == "estado":
            if message.payload in (b"conectado", b"desconectado"):
                self.on_status(device_id, message.payload.decode("ascii"))
            return
        if kind not in ("tratado", "crudo"):
            return
        if len(message.payload) > MAX_PAYLOAD_BYTES:
            LOGGER.warning("Se descartó una lectura MQTT demasiado grande de %s", device_id)
            return
        try:
            self.messages.put_nowait((message.topic, device_id, message.payload))
        except queue.Full:
            LOGGER.warning("Cola MQTT llena, se descartó un mensaje de %s", device_id)

    def process_messages(self) -> None:
        while not self.stopping.is_set():
            try:
                topic, device_id, payload = self.messages.get(timeout=0.25)
            except queue.Empty:
                continue

            try:
                if topic.endswith("/crudo"):
                    converted = convert_csv_message(topic, payload)
                    document = json.loads(converted.json_payload)
                    document["record_id"] = converted.record_id
                    self.save_reading(device_id, document)
                    self.confirm_replay(device_id, document)
                else:
                    self.save_reading(device_id, json.loads(payload))
            except (KeyError, TypeError, ValueError, OverflowError, RecursionError, csv.Error) as error:
                LOGGER.warning("Se descartó una lectura MQTT inválida de %s: %s", device_id, error)
            except (psycopg.Error, RuntimeError):
                LOGGER.exception("No se pudo procesar la lectura MQTT de %s", device_id)
            finally:
                self.messages.task_done()

    def save_reading(self, device_id: str, document: dict) -> None:
        if insert_reading(self.pool, parse_reading(device_id, document)):
            self.on_reading()

    def confirm_replay(self, device_id: str, document: dict) -> None:
        if self.stopping.is_set() or not self.client.is_connected():
            return

        prefix = f"dispositivo/{device_id}"
        publication = self.client.publish(
            f"{prefix}/tratado", json.dumps(document, ensure_ascii=False, allow_nan=False), qos=1,
        )
        if publication.rc == mqtt.MQTT_ERR_SUCCESS:
            publication.wait_for_publish(timeout=PUBLISH_TIMEOUT_SECONDS)
            if publication.is_published():
                acknowledgment = self.client.publish(f"{prefix}/ack", document["record_id"], qos=1)
                if acknowledgment.rc == mqtt.MQTT_ERR_SUCCESS:
                    return
        LOGGER.warning("Confirmación MQTT pendiente para %s", device_id)
